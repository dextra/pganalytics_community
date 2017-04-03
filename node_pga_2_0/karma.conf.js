module.exports = function(config) {
	config.set({
		files : [
			'site/bower_components/angular/angular.js',
			'site/bower_components/angular-mocks/angular-mocks.js',
			'site/bower_components/jquery/dist/jquery.min.js',
			'site/bower_components/moment/moment.js',
			'site/bower_components/sideshow/distr/dependencies/*.js',
			'site/bower_components/sideshow/distr/sideshow.min.js',
			'site/bower_components/d3/d3.min.js',
			'site/bower_components/nvd3/nv.d3.min.js',
			'site/bower_components/angular-nvd3/dist/angular-nvd3.min.js',
			'site/bower_components/bootstrap/dist/js/bootstrap.min.js',
			'site/bower_components/angular-bootstrap/ui-bootstrap.min.js',
			'site/bower_components/angular-bootstrap-datetimepicker/js/datetimepicker.js',
			'site/js/tutorials/sideshow.config.js',
			'site/js/angular/*.js',
			'site/js/directives/*.js',
			'site/js/services/*.js',
			'site/js/utils.js',
			'site/js/controllers/mainController.js',
			'site/js/controllers/dashboardController.js',
			'test/unit/*.js'
		],
		preprocessors : {
			'site/js/**/*.js': ['coverage']
		},
		plugins : [
			'karma-chrome-launcher',
			'karma-firefox-launcher',
			'karma-phantomjs-launcher',
			'karma-jasmine',
			'karma-notify-reporter',
			'karma-junit-reporter',
			'karma-coverage'
		],
		basePath: 'pga/',
		frameworks: ['jasmine'],
		browsers: ['PhantomJS'],
		autoWatch: true,
		singleRun: false,
		colors: true,
		junitReporter : {
			outputFile: 'test/out/testresults/unit.xml',
			suite: 'unit'
		},
		reporters: ['junit', 'progress', 'notify', 'coverage'],
		// notify-reporter settings
		notifyReporter: {
			reportEachFailure: true, // Default: false, Will notify on every failed sepc 
			reportSuccess: true, // Default: true, Will notify when a suite was successful 
		},
		coverageReporter: {
			type : 'html',
			dir : 'test/out/codecoverage/'
		}
	});
};

