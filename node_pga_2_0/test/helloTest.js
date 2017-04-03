var t = require('qunitjs');

exports.load = function() {

	t.test('sample1', function(a) {
		a.equal(1, 1);
	});

	t.test('sample2', function(a) {
		a.equal(1, 1);
	});
	
}