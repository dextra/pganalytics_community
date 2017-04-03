function mountDataTableExhibition(json, tableDom, rowClickCallback, serverSideProcessingURL, extra_options) {
	var $div = $(tableDom);
	var $tbl = $div.data('dataTableComponent');
	if (!extra_options)
		extra_options = {};
	if ($tbl) {
		/* Destroy current dataTable components */
		$tbl.dataTable().fnDestroy();
		/* Remove DOM elements */
		/* XXX: We could use `$div.children().remove()` but it takes considerable longer time for big tables. */
		for (var i = 0; i < tableDom.size(); i++) {
			for (var j = tableDom.get(i).childNodes.length - 1; j >= 0; j--) {
				tableDom.get(i).removeChild(tableDom.get(i).childNodes[j]);
			}
		}
	}
	$tbl = $('<table class="table table-striped table-hover data-table-parent-component ' + (rowClickCallback ? ' clickable-rows' : '') + '" style="width:100%"></table>').appendTo($div);
	$div.data('dataTableComponent', $tbl);
	/* drawCallback and rowCallback are used internally, so save the users ones (if any) to be called afterwards */
	var callers_DrawCallback = extra_options.drawCallback;
	var callers_RowCallback = extra_options.rowCallback;
	delete extra_options.drawCallback;
	delete extra_options.rowCallback;
	var options = {
			"language": {
				"sEmptyTable": "Nenhum registro encontrado",
				"sInfo": "Mostrando de _START_ até _END_ de _TOTAL_ registros",
				"sInfoEmpty": "Mostrando 0 até 0 de 0 registros",
				"sInfoFiltered": "(Filtrados de _MAX_ registros)",
				"sInfoPostFix": "",
				"sInfoThousands": ".",
				"sLengthMenu": "_MENU_ resultados por página",
				"sLoadingRecords": "Carregando...",
				"sProcessing": "Carregando dados <span class='loading-component'></span>",
				"sZeroRecords": "Nenhum registro encontrado",
				"sSearch": "Pesquisar: ",
				"oPaginate": {
					"sNext": "Próximo",
					"sPrevious": "Anterior",
					"sFirst": "Primeiro",
					"sLast": "Último"
				},
				"oAria": {
					"sSortAscending": ": Ordenar colunas de forma crescente",
					"sSortDescending": ": Ordenar colunas de forma decrescente"
				}
			},
			"bProcessing": true,
			"bPaginate": true,
			"sPaginationType": "full_numbers",
			"scrollX": true,
			"searchDelay": 1000,
			"dom": '<"top">frt<"dataTable-navigate"l><"dataTable-information"i><"dataTable-pages"p><"clearfix">',
			"aoColumns": json.columns,
			"fnRowCallback": function(nRow, aData, iDisplayIndex, iDisplayIndexFull) {
				if (rowClickCallback && !$(nRow).data('pga-defined-click')) {
					$(nRow)
						.data('pga-defined-click', true)
						.on('click', function() {
							rowClickCallback.apply(this, [aData, nRow, iDisplayIndex, iDisplayIndexFull]);
						});
				}
				if (callers_RowCallback) {
					callers_RowCallback.apply(this, [nRow, aData, iDisplayIndex, iDisplayIndexFull]);
				}
			},
			"fnDrawCallback": function(oSettings) {
				var pos = 1;
				$tbl.find('thead th').each(function() {
					$tbl.find('tbody tr td:nth-of-type(' + pos + ')').attr('data-title', $(this).text());
					pos++;
				});
				if (callers_DrawCallback) {
					callers_DrawCallback.apply(this, [oSettings]);
				}
			}
	};
	$.extend(options, extra_options);
	if (!serverSideProcessingURL) {
		options["aaData"] = json.rows;
	} else {
		options["ajax"] = serverSideProcessingURL;
		options["serverSide"] = true;
	}
	$tbl.dataTable(options);
}

function sqlModal(args, settings, aData, nRow, iDisplayIndex, iDisplayIndexFull) {
	var sql_command = aData[$(this).parents("table:first").dataTable().api().column(args.display + ":name").index()];
	/* Mark the row as selected, if not already */
	if (!$(nRow).is(".selected")) {
		$(nRow).parents("table:first").find("tr").removeClass("selected");
		$(nRow).addClass("selected");
	}
	triggerSqlModal(sql_command);
}

