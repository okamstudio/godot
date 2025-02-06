const fs = require('fs');
const globals = require('globals');
const htmlParser = require('@html-eslint/parser');
const htmlPlugin = require('@html-eslint/eslint-plugin');
const pluginJs = require('@eslint/js');
const pluginReference = require('eslint-plugin-html');
const stylistic = require('@stylistic/eslint-plugin');
const pluginIgnoreCPreprocessors = require("@godotengine/eslint-plugin-ignore-c-preprocessors").default;

if (process && process.env && process.env.npm_command && !fs.existsSync('./platform/web/eslint.config.cjs')) {
	throw Error('eslint must be run from the Godot project root folder');
}

const emscriptenGlobals = {
	'Browser': true,
	'ERRNO_CODES': true,
	'FS': true,
	'GL': true,
	'HEAP32': true,
	'HEAP8': true,
	'HEAPF32': true,
	'HEAPU8': true,
	'IDBFS': true,
	'LibraryManager': true,
	'MainLoop': true,
	'Module': true,
	'UTF8ToString': true,
	'_emscripten_webgl_get_current_context': true,
	'_free': true,
	'_malloc': true,
	'autoAddDeps': true,
	'getValue': true,
	'lengthBytesUTF8': true,
	'mergeInto': true,
	'runtimeKeepalivePop': true,
	'runtimeKeepalivePush': true,
	'setValue': true,
	'stringToUTF8': true,
	'stringToUTF8Array': true,
	'wasmTable': true,
};

module.exports = [
	pluginJs.configs.all,
	stylistic.configs.customize({ jsx: false }),

	{
		rules: {
			'consistent-this': ['error', 'me'], // enforce consistent naming when capturing the current execution context
			'curly': ['error', 'all'], // enforce consistent brace style for all control statements
			'no-else-return': ['error', { 'allowElseIf': true }], // disallow else blocks after return statements in if statements
			'no-param-reassign': ['error', { 'props': false }], // disallow reassigning function parameters
			'no-unused-vars': ['error', { 'args': 'none', 'caughtErrors': 'none' }], // disallow unused variables

			'@stylistic/arrow-parens': ['error', 'always'], // enforces the consistent use of parentheses in arrow functions
			'@stylistic/brace-style': ['error', '1tbs', { 'allowSingleLine': false }], // describes the placement of braces relative to their control statement and body
			'@stylistic/comma-dangle': ['error', {
				'arrays': 'always-multiline',
				'objects': 'always-multiline',
				'imports': 'always-multiline',
				'exports': 'always-multiline',
				'functions': 'never',
			}], // enforces consistent use of trailing commas in object and array literals
			'@stylistic/indent': ['error', 'tab', { 'SwitchCase': 0 }], // enforces a consistent indentation style
			'@stylistic/indent-binary-ops': ['error', 'tab'], // indentation for binary operators in multiline expressions
			'@stylistic/multiline-ternary': ['error', 'always-multiline'], // enforces or disallows newlines between operands of a ternary expression
			'@stylistic/no-tabs': ['error', { 'allowIndentationTabs': true }], // looks for tabs anywhere inside a file: code, comments or anything else
			'@stylistic/quote-props': ['error', 'consistent'], // requires quotes around object literal property names
			'@stylistic/quotes': ['error', 'single'], // enforces the consistent use of either backticks, double, or single quotes
			'@stylistic/semi': ['error', 'always'], // enforces consistent use of semicolons
			'@stylistic/spaced-comment': ['error', 'always', { 'block': { 'exceptions': ['*'] } }], // enforce consistency of spacing after the start of a comment

			'camelcase': 'off', // disable: camelcase naming convention
			'capitalized-comments': 'off', // disable: enforce or disallow capitalization of the first letter of a comment
			'complexity': 'off', // disable: enforce a maximum cyclomatic complexity allowed in a program
			'dot-notation': 'off', // disable: enforce dot notation whenever possible
			'eqeqeq': 'off', // disable: require the use of === and !==
			'func-name-matching': 'off', // disable: require function names to match the name of the variable or property to which they are assigned
			'func-names': 'off', // disable: checking named function expressions
			'func-style': 'off', // disable: consistent use of either function declarations or expressions
			'id-length': 'off', // disable: enforce minimum and maximum identifier lengths
			'init-declarations': 'off', // disable: require or disallow initialization in variable declarations
			'line-comment-position': 'off', // disable: enforce position of line comments
			'max-classes-per-file': 'off', // disable: maximum number of classes per file
			'max-lines': 'off', // disable: maximum number of lines per file
			'max-lines-per-function': 'off', // disable: maximum number of lines of code in a function
			'max-params': 'off', // disable: enforce a maximum number of parameters in function definitions
			'max-statements': 'off', // disable: maximum number of statements allowed in function blocks
			'multiline-comment-style': 'off', // disable: enforce a particular style for multiline comments
			'new-cap': 'off', // disable: require constructor names to begin with a capital letter
			'no-bitwise': 'off', // disable: disallow bitwise operators
			'no-continue': 'off', // disable: disallow continue statements
			'no-empty-function': 'off', // disable: disallow empty functions
			'no-eq-null': 'off', // disable: disallow null comparisons without type-checking operators
			'no-implicit-coercion': 'off', // disable: disallow shorthand type conversions
			'no-inline-comments': 'off', // disable: disallow inline comments after code
			'no-magic-numbers': 'off', // disable: disallow magic numbers
			'no-negated-condition': 'off', // disable: disallow negated conditions
			'no-plusplus': 'off', // disable: disallow the unary operators ++ and --
			'no-self-assign': 'off', // disable: disallow assignments where both sides are exactly the same
			'no-ternary': 'off', // disable: disallow ternary operators
			'no-undefined': 'off', // disable: disallow the use of undefined as an identifier
			'no-underscore-dangle': 'off', // disable: disallow dangling underscores in identifiers
			'no-useless-assignment': 'off', // disable: disallow variable assignments when the value is not used
			'no-warning-comments': 'off', // disable: disallow specified warning terms in comments
			'object-shorthand': 'off', // disable: require or disallow method and property shorthand syntax for object literals
			'one-var': 'off', // disable: enforce variables to be declared either together or separately in functions
			'prefer-arrow-callback': 'off', // disable: require using arrow functions for callbacks
			'prefer-destructuring': 'off', // disable: require destructuring from arrays and/or objects
			'prefer-named-capture-group': 'off', // disable: enforce using named capture group in regular expression
			'prefer-promise-reject-errors': 'off', // disable: require using Error objects as Promise rejection reasons
			'prefer-rest-params': 'off', // disable: require rest parameters instead of arguments
			'prefer-spread': 'off', // disable: require spread operators instead of .apply()
			'require-unicode-regexp': 'off', // disable: enforce the use of u or v flag on RegExp
			'sort-keys': 'off', // disable: require object keys to be sorted
		},
	},

	// jsdoc2rst (node)
	{
		files: ['js/jsdoc2rst/**/*.js', 'platform/web/js/jsdoc2rst/**/*.js'],
		languageOptions: {
			globals: globals.node,
		},
	},

	// engine files (browser)
	{
		files: ['js/engine/**/*.js', 'platform/web/js/engine/**/*.js'],
		languageOptions: {
			globals: {
				...globals.browser,
				'Features': true,
				'Godot': true,
				'InternalConfig': true,
				'Preloader': true,
			},
		},
	},

	// libraries and modules (browser)
	{
		files: ['js/libs/**/*.js', 'platform/web/js/libs/**/*.js', 'modules/**/*.js'],
		plugins: {
			"ignore-c-preprocessors": pluginIgnoreCPreprocessors
		},
		processor: "ignore-c-preprocessors/ignore-c-preprocessors",
		languageOptions: {
			globals: {
				...globals.browser,
				...emscriptenGlobals,
				'GodotConfig': true,
				'GodotEventListeners': true,
				'GodotFS': true,
				'GodotOS': true,
				'GodotAudio': true,
				'GodotRuntime': true,
				'IDHandler': true,
				'XRWebGLLayer': true,
			},
		},
	},

	// javascript templates (service workers)
	{
		files: ['misc/dist/html/**/*.js'],
		languageOptions: {
			globals: {
				...globals.browser,
				'___GODOT_CACHE___': true,
				'___GODOT_ENSURE_CROSSORIGIN_ISOLATION_HEADERS___': true,
				'___GODOT_OPT_CACHE___': true,
			},
		},
	},

	// html templates
	{
		files: ['misc/dist/html/**/*.html'],
		plugins: {
			'@html-eslint': htmlPlugin,
			'eslint-plugin-html': pluginReference,
		},
		languageOptions: {
			parser: htmlParser,
			globals: {
				...globals.browser,
				'Engine': true,
				'$GODOT_CONFIG': true,
				'$GODOT_PROJECT_NAME': true,
				'$GODOT_THREADS_ENABLED': true,
				'___GODOT_THREADS_ENABLED___': true,
			},
		},
		rules: {
			...htmlPlugin.configs.recommended.rules,
			'@html-eslint/indent': ['error', 'tab'],
			'@html-eslint/require-closing-tags': ['error', { 'selfClosing': 'never' }],
			'no-alert': 'off',
			'no-console': 'off',
		},
	},

	{
		ignores: [
			'**/eslint.config.cjs',
			'**/.eslintrc*.js',
			'**/*.externs.js',
		],
	},
];
