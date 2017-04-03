$(document).ready(function() {
	var activeNavClass = "navmenu-toggle-clicked";
	$('<div class="mask"></div>').appendTo('body');
	$(".nav-toggler").on('click', function() {
		$("body").toggleClass(activeNavClass);
	});
	$('.menu-controle a,#analise-sql-modal button,.mask').on('click', function() {
		$("body").removeClass(activeNavClass);
	});
});

