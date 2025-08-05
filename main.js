/* --- setup rendering context --- */

/** @type {HTMLCanvasElement} */
const canvas = document.getElementById('canvas');
canvas.width = 140;
canvas.height = 140;
const gl = canvas.getContext('webgl');

/* --- setup vertex buffer --- */

const vertexBuffer = gl.createBuffer();
gl.bindBuffer(gl.ARRAY_BUFFER, vertexBuffer);
gl.bufferData(gl.ARRAY_BUFFER, 1024*1024, gl.DYNAMIC_DRAW);

/* --- compile shader program --- */

const vs = gl.createShader(gl.VERTEX_SHADER);
const fs = gl.createShader(gl.FRAGMENT_SHADER);
gl.shaderSource(vs, document.getElementById('vert-shader').textContent);
gl.shaderSource(fs, document.getElementById('frag-shader').textContent);
gl.compileShader(vs);
gl.compileShader(fs);
const program = gl.createProgram();
gl.attachShader(program, vs);
gl.attachShader(program, fs);
gl.linkProgram(program);
gl.deleteShader(vs);
gl.deleteShader(fs);
gl.useProgram(program);

/* --- setup shader attributes --- */

const a_dst_pos = gl.getAttribLocation(program, 'a_dst_pos');
const a_src_pos = gl.getAttribLocation(program, 'a_src_pos');
const a_color = gl.getAttribLocation(program, 'a_color');
gl.enableVertexAttribArray(a_dst_pos);
gl.enableVertexAttribArray(a_src_pos);
gl.enableVertexAttribArray(a_color);
gl.vertexAttribPointer(a_dst_pos, 2, gl.FLOAT, false, 8*4, 0);
gl.vertexAttribPointer(a_src_pos, 2, gl.FLOAT, false, 8*4, 2*4);
gl.vertexAttribPointer(a_color, 4, gl.FLOAT, false, 8*4, 4*4);

/* --- load the texture --- */

const texture = gl.createTexture();
gl.bindTexture(gl.TEXTURE_2D, texture);
gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
const atlasElement = document.getElementById('atlas');
gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, atlasElement);

/* --- get uniform locations --- */

const u_resolution = gl.getUniformLocation(program, 'u_resolution');
const u_texture = gl.getUniformLocation(program, 'u_texture');

/* --- configure capabilities --- */

gl.disable(gl.DEPTH_TEST);
gl.enable(gl.BLEND);
gl.blendFunc(gl.ONE, gl.ONE_MINUS_SRC_ALPHA);

let lastTimestamp = 0;
function slowLog(timestamp, ...args) {
	if (timestamp - lastTimestamp < 1000) {
		return;
	}
	lastTimestamp = timestamp;
	console.log(...args);
}

async function main() {
	const { instance } = await WebAssembly.instantiateStreaming(fetch('main.wasm'));
	const canvasRect = canvas.getBoundingClientRect();
	canvas.addEventListener('click', (e) => {
		const scale = canvasRect.width / canvas.width;
		const x = e.offsetX / scale;
		const y = e.offsetY / scale;
		instance.exports.on_mouse_click(x, y, false);
	});
	canvas.addEventListener('contextmenu', (e) => {
		e.preventDefault();
		const scale = canvasRect.width / canvas.width;
		const x = e.offsetX / scale;
		const y = e.offsetY / scale;
		instance.exports.on_mouse_click(x, y, true);
	});
	const seed = Math.floor(Math.random() * (1 << 31))
	instance.exports.initialize(seed);
	/** @type {ArrayBuffer} */
	const memory = instance.exports.memory.buffer;
	const vertsAddr = instance.exports.vertices.value;
	const mainLoop = (timestamp) => {
		const vertsCount = instance.exports.next_frame(timestamp);
		const size = vertsCount * 8*4;
		slowLog(timestamp, size);
		const dataview = new DataView(memory, vertsAddr, size);
		gl.bufferSubData(gl.ARRAY_BUFFER, 0, dataview);
		gl.uniform2f(u_resolution, canvas.width, canvas.height);
		gl.uniform1i(u_texture, 0)
		gl.clearColor(0,0,0,1);
		gl.clear(gl.COLOR_BUFFER_BIT);
		gl.drawArrays(gl.TRIANGLES, 0, vertsCount);
		requestAnimationFrame(mainLoop);
	}
	requestAnimationFrame(mainLoop);
}

main();
