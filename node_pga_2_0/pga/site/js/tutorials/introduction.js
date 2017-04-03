Sideshow.registerWizard({
	name: "introduction",
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
		}
	}
}).storyLine({
	showStepPosition: true,
	steps: addNavigation([
		{
			title: "Escolha o Servidor PostgreSQL",
			text: "O pgAnalytics pode armazenar dados de vários servidores, várias instâncias e várias bases de dados.\n"
			      + "É necessário selecionar ao menos o servidor e a instância ao qual deseja analisar. Opcionalmente especifique a base de dados ou a selecione \"**Todas as bases**\" para uma análise geral.",
			subject: "#select-db",
			targets: "#select-db button"
		},
		{
			title: "Defina o período de tempo",
			text:
				"Especifique a data/hora inicial e final para filtrar os dados que serão apresentados na tela. O padrão considera as últimas 8 horas de dados disponíveis.\n"
				+ "A qualquer momento é possível alterar o período, as informações serão atualizadas automaticamente para o período especificado:\n"
				+ "\n"
				+ "* Clique no ícone <span class='fa fa-calendar'></span> para abrir o calendário de datas.\n"
				+ "* Clique em <span class='glyphicon glyphicon-chevron-up'></span>/<span class='glyphicon glyphicon-chevron-down'></span> para navegar nas horas.\n"
				+ "* Ou então digite os valores de data/hora direto no componente.\n"
				+ "\n"
				+ "Os controles à direita podem ser usados como atalhos na navegação:\n"
				+ "\n"
				+ "* **<span class='fa fa-backward'></span> 1h**: move tanto a data/hora inicial quanto final para 1 hora atrás\n"
				+ "* **8 horas**: navega direto para as últimas 8 horas de dados disponíveis\n"
				+ "* **1 dia**: navega direto para as últimas 24 horas de dados disponíveis\n"
				+ "* **1 semana**: navega direto para a última semana de dados disponíveis\n"
				+ "* **1h <span class='fa fa-forward'></span>**: move tanto a data/hora inicial quanto final para 1 hora à frente\n"
				+ "\n"
				,
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
			title: "Navegue pelas informações",
			text:
				"Neste menu estão as seções e subseções que agrupam diversas informações:\n"
				+ "\n"
				+ "* <span class='fa fa-eye'></span> **Visão Geral**: Informações sobre alertas gerados através da análise automatica do PgAnalytics \n"
				+ "* <span class='fa fa-database'></span> **Estatísticas do PostgreSQL**: Informações sobre todos os objetos do PostgreSQL \n"
				+ "* <span class='fa fa-desktop'></span> **Sistema Operacional**: Informações sobre os principais aspectos do sistema operacional\n"
				+ "* <span class='fa fa-cogs'></span> **Análise de comandos SQL**: Informações consolidadas e detalhadas sobre os comandos SQL executados no período\n"
				+ "* <span class='fa fa-search'></span> **Análise de logs**: Pesquisa livre sobre o log do PostgreSQL\n"
				+ "* <span class='fa fa-hdd-o'></span> **Backups executados**: Catálogo com diversas informações sobre os backups executados \n"
				+ "* <span class='fa fa-comments'></span> **Diagnósticos e recomendações**: Lista de diagnósticos e recomendações fornecidas pelos DBAs da Dextra (apenas para o pacote Expert)\n"
				,
			subject: "#main-menu"
		},
		{
			title: "Navegue pelas informações",
			text:
				"Após selecionar a seção e/ou subseção, clique nesse componente para selecionar outras informações para análise nesse mesmo contexto. Existem três tipos de elementos:\n"
				+ "\n"
				+ "* **<span class='fa fa-table'></span> Tabelas:** exibem várias informações em formato tabular, você pode clicar nas colunas para mudar a ordenação, pesquisar e em algumas tabelas clicar na linha para ver mais detalhes e outros gráficos\n"
				+ "* **<span class='fa fa-area-chart'></span> Gráfico de área**: exibem sempre no eixo X a linha do tempo e eixo Y as informações, acumulando os valores na exibição\n"
				+ "* **<span class='fa fa-line-chart'></span> Gráfico de linha**: como o gráfico de área, mas não acumula os valores, podendo ter linhas intercaladas"
				+ "\n"
				,
			subject: "#select-charts-dropdown"
		},
		{
			title: "Analise as informações",
			text:
				"Nessa área são apresentados os gráficos e tabelas:\n"
				+ "\n"
				+ "* Clique em **Absoluto/Porcentual** para alterar a visualização dos dados\n"
				+ "* Clique na legenda para exibir ou esconder determinadas informações\n"
				+ "* Ao passar o mouse sobre o gráfico, um tooltip com os dados reais do período será exibido\n"
				+ "* Clique em <span class='fa fa-save'></span> para exportar a visualização\n"
				+ "* Clique em <span class='fa fa-table'></span> para alterar a visualização para tabela e em <span class='fa fa-line-chart'></span> para retornar ao gráfico"

				,
			subject: "#main-widget"
		}
	])
});

