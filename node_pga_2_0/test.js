var t = require('qunitjs');
var testutil = require('./test/util/testutil');

t.testStart(function(details) {
	console.info('Test [', details.module, ']', details.name, ': started');
});

t.log(function(details) {
	if (!details.result) {
		console.error('Assert [', details.module, ']', details.name, 'expected:', JSON.stringify(details.expected), ', but was:', JSON.stringify(details.actual));
		if (details.message) {
			console.error(details.message);
		}
		console.error(details.source);
	}
});

t.testDone(function(details) {
	var result = details.failed ? 'failed' : 'success';
	console.info('Test [', details.module, ']', details.name, ':', result, '(', details.assertions.length, 'asserts )');
});

t.begin(function() {
	testutil.begin();
});

t.done(function(result) {
	testutil.done(result);
	console.info('Total:', result.total, 'success:', result.passed, 'failed:', result.failed)
	console.info('BUILD', (result.falied ? 'FAILED' : 'SUCCESS'));
	process.exit(result.failed);
});

function load(name) {
	var mod = require('./test/' + name + 'Test');
	t.module(name);
	mod.load();
}

load('hello');
load('static');
load('sql');

t.load();
