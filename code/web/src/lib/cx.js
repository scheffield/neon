// from https://github.com/preactjs/preact-www/blob/master/src/lib/cx.js
export default function cx() {
	let out = '';
	for (let i = 0; i < arguments.length; i++) {
		const x = arguments[i];
		if (out) out += ' ';
		if (x) out += x;
	}
	return out;
}