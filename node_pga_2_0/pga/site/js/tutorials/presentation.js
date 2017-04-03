Sideshow.registerWizard({
	name: "presentation",
	title: "Conheça o pgAnalytics",
	description: "Veja os principais elementos da aplicação neste rápido tutorial.",
	estimatedTime: "10 Minutos",
	affects: [
		function(){
			//Here we could do any checking to infer if this tutorial is eligible the current screen/context. 
			//After this checking, just return a boolean indicating if this tutorial will be available. 
			//As a simple example we're returning a true, so this tutorial would be available in any context
			return true;
		}
	],
	listeners: {
		beforeWizardStarts: function() {
			$('.sostats > a').click();
			$('.time-move-8h').click();
		}
	}
}).storyLine({
	showStepPosition: false,
	steps: [
		{
			title: "pgAnalytics",
			text: "<center><strong>Facilitando a vida do DBA</strong><br /><br />Visão Geral do Sistema<center>",
			format: "markdown"
		},
		{
			title: "Análise e monitoramento de diversos servidores",
			text: "todos servidores, todas instâncias e todos bancos de dados",
			subject: "#select-db"
		},
		{
			title: "Análise temporal do ambiente",
			text: "Navegue no tempo, e tenha uma visão ambiente em diversos momentos",
			subject: "#periodo",
			listeners: {
				beforeStep: function() {
					$("html, body").scrollTop(0);
					if (!$(".area-periodo .datepicker-row").is(":visible")) {
						$(".area-periodo .toggle-datepicker").click();
					}
				}
			}
		},
		{
			title: "Diversas métricas coletadas",
			text: "<strong>Recursos físicos:</strong> CPU, memória, disco, e muito mais...",
			subject: "#main-menu",
			format: "markdown"
		},
		{
			title: "Diversas métricas coletadas",
			text: "<strong>Estatísticas do PostgreSQL:</strong> cache, checkpoint, indexação, arquivos temporários, e muito mais...",
			subject: "#main-menu",
			format: "markdown",
			listeners: {
				beforeStep: function() {
					$(".pgstats > a:first").click();
				}
			}
		},
		/*
		{
			title: "Diversas métricas coletadas",
			text: "",
			subject: "#select-charts-dropdown"
		},
		*/
		{
			title: "Gráficos fáceis e intuitivos",
			text: "",
			subject: "#main-widget"
		},
		{
			title: "Análise de comandos SQL executados",
			text: "Verifique em detalhes os comandos SQL que causam maior impacto de performance do PostgreSQL",
			subject: "#main-widget",
			listeners: {
				beforeStep: function() {
					location.hash = location.hash
						.replace(/section=[a-z_\.]+/, 'section=sql')
						.replace(/chart=[a-z_\.]+/, 'chart=pgstatements_normalized_list');
				}
			}
		},
		{
			title: "Detalhes das execuções de comandos SQL",
			text: "Veja cada uma das execuções ou uma análise gráfica temporal",
			subject: "#main-widget",
			listeners: {
				beforeStep: function() {
					location.hash = location.hash
						.replace(/section=[a-z_\.]+/, 'section=sql.details')
						.replace(/chart=[a-z_\.]+/, 'chart=pgstatements_detail_list')
						.replace(/\&filter=.*$/, '') + '&filters={%22statement_id%22%3A%2213%22}';
				}
			}
		},
		{
			title: "pgAnalytics",
			text: "<center><strong>Facilitando a vida do DBA</strong><br /><br />Faça o teste você mesmo: http://<span class='text-logo-white'><span class='logo-pg'>pg</span><span class='logo-analytics'>Analytics</span></span>.io/demo/<center>",
			format: "markdown"
		}

	]
});

