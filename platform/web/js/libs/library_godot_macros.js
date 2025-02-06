/* eslint-disable */
{{{
/* eslint-enable */
	globalThis.___EMSCRIPTEN_VERSION_PARSED = globalThis.EMSCRIPTEN_VERSION.split('.').map((n) => parseInt(n, 10));

	globalThis.___SEMVER_IS_GREATER_THAN = (a, b, { orEqual = false } = {}) => {
		const [aMajor, aMinor, aPatch] = a;
		const [bMajor, bMinor, bPatch] = b;
		if ((orEqual && aMajor >= bMajor) || aMajor > bMajor) {
			if ((orEqual && aMinor >= bMinor) || aMinor > bMinor) {
				if ((orEqual && aPatch >= bPatch) || aPatch > bPatch) {
					return true;
				}
			}
		}
		return false;
	};

	globalThis.EMSCRIPTEN_VERSION_IS_GREATER_THAN = (major, minor, patch) => {
		const isGreater = globalThis.___SEMVER_IS_GREATER_THAN(globalThis.___EMSCRIPTEN_VERSION_PARSED, [major, minor, patch]);
		return isGreater;
	};

	globalThis.EMSCRIPTEN_VERSION_IS_GREATER_THAN_OR_EQUAL = (major, minor, patch) => {
		const isGreaterOrEqual = globalThis.___SEMVER_IS_GREATER_THAN(globalThis.___EMSCRIPTEN_VERSION_PARSED, [major, minor, patch], { orEqual: true });
		return isGreaterOrEqual;
	};

	globalThis.EMSCRIPTEN_VERSION_IS_LESS_THAN = (major, minor, patch) => {
		const isGreaterOrEqual = globalThis.___SEMVER_IS_GREATER_THAN(globalThis.___EMSCRIPTEN_VERSION_PARSED, [major, minor, patch], { orEqual: true });
		return !isGreaterOrEqual;
	};

	globalThis.EMSCRIPTEN_VERSION_IS_LESS_THAN_OR_EQUAL = (major, minor, patch) => {
		const isGreater = globalThis.___SEMVER_IS_GREATER_THAN(globalThis.___EMSCRIPTEN_VERSION_PARSED, [major, minor, patch]);
		return !isGreater;
	};
/* eslint-disable */
}}}
/* eslint-enable */
