/* Draggable */

(function($) {
	/* Source: http://css-tricks.com/snippets/jquery/draggable-without-jquery-ui/ */
	$.fn.drags = function(opt) {
		opt = $.extend({handle:"",cursor:"move"}, opt);
		if(opt.handle === "") {
			var $el = this;
		} else {
			var $el = this.find(opt.handle);
		}
		return $el.css('cursor', opt.cursor).on("mousedown", function(e) {
			if(opt.handle === "") {
				var $drag = $(this).addClass('draggable');
			} else {
				var $drag = $(this).addClass('active-handle').parent().addClass('draggable');
			}
			var z_idx = $drag.css('z-index'),
				drg_h = $drag.outerHeight(),
				drg_w = $drag.outerWidth(),
				pos_y = $drag.offset().top + drg_h - e.pageY,
				pos_x = $drag.offset().left + drg_w - e.pageX;
			$drag.css('z-index', 1000).parents().on("mousemove", function(e) {
				$('.draggable').offset({
					top:e.pageY + pos_y - drg_h,
					left:e.pageX + pos_x - drg_w
				}).on("mouseup", function() {
					$(this).removeClass('draggable').css('z-index', z_idx);
				});
			});
			e.preventDefault(); // disable selection
		}).on("mouseup", function() {
			if(opt.handle === "") {
				$(this).removeClass('draggable');
			} else {
				$(this).removeClass('active-handle').parent().removeClass('draggable');
			}
		});
	}
})(jQuery);

/* Sideshow config */
Sideshow.config.language = "pt-br";
Sideshow.init();

Sideshow.gotoNext = function() {
	$('.sideshow-next-step-button').click();
};

Sideshow.gotoPrevious = function() {
	var steps = $('.sideshow-step-position').text().match(/^([0-9]+)\/([0-9]+)/);
	if (steps && steps[1] && steps[2]) {
		var curr = parseInt(steps[1]);
		var total = parseInt(steps[2]);
		if (!isNaN(curr) && !isNaN(total) && curr > 1) {
			Sideshow.gotoStep(curr - 1);
		} else if (curr == 1) {
			Sideshow.close();
		}
	}
};

window.__pga_start_presentation = function() {
	Sideshow.runWizard('presentation');
	$('.sideshow-next-step-button,.sideshow-step-position').hide();
	$(".sideshow-step-description").drags();
	$('.sideshow-step-description').css({
		//'max-width': '500px',
		'text-align': 'center'
		});
	$('<div style="text-align:right;color:#f0f0f0;font-size:1.3em">http://<span class="text-logo-white"><span class="logo-pg">pg</span><span class="logo-analytics">Analytics</span></span>.io/</div>')
		.appendTo(".sideshow-step-description");
};

$('html').on('keypress', function(evt) {
	if (evt.keyCode == 39) { // right arrow
		Sideshow.gotoNext();
		$('.modal').modal('hide');
	} else if (evt.keyCode == 37) { // left arrow
		Sideshow.gotoPrevious();
		$('.modal').modal('hide');
	} else if (evt.keyCode == 27) { // ESC
		//Sideshow.close();
	} else if (evt.key.toLowerCase() == 'm' && evt.altKey) { // Alt+M (hide/show sideshow without stopping it)
		var elements = '.sideshow-step-description,.sideshow-mask-corner-part,.sideshow-mask-part';
		if ($('.sideshow-step-description').is(':visible')) {
			$(elements).fadeOut('slow');
		} else {
			$(elements).fadeIn('slow');
		}
	} else if (evt.key.toLowerCase() === 'p' && evt.altKey) { // Alt+P (start "presentation")
		window.__pga_start_presentation();
	}
});

$(document).ready(function() {
	$("#run_sideshow").click(function(){
		//Sideshow.start({ listAll: true });
		Sideshow.runWizard('introduction');
		$('<button type="button" class="close" aria-label="Fechar" onclick="Sideshow.close();return false"><span aria-hidden="true">&times;</span></button>')
			.prependTo(".sideshow-step-description");
		$('<button type="button" class="sideshow-previous-step-button btn btn-default btn-sm" onclick="Sideshow.gotoPrevious();return false">Voltar</button>')
			.appendTo(".sideshow-step-description");
		$(".sideshow-step-description").drags();
	});
});

function addNavigation(steps) {
	var stepnavlinks = "\n<hr>Ir para: ";
	for (var i = 0; i < steps.length; i++) {
		if (i != 0) stepnavlinks += ', ';
		stepnavlinks += "[" + (i+1) + "](javascript:Sideshow.gotoStep(" + (i+1) + ") \"" + steps[i].title + "\") ";
	}
	for (var i = 0; i < steps.length; i++) {
		if (!steps[i].format) steps[i].format = "markdown";
		steps[i].text += stepnavlinks;
	}
	return steps;
}

