function mountTableWithReaction (json, tableDom, selectedAct) {
	var tabela = $(tableDom).DataTable({
   		"language": {
		    "sEmptyTable": "Nenhum registro encontrado",
		    "sInfo": "Mostrando de _START_ até _END_ de _TOTAL_ registros",
		    "sInfoEmpty": "Mostrando 0 até 0 de 0 registros",
		    "sInfoFiltered": "(Filtrados de _MAX_ registros)",
		    "sInfoPostFix": "",
		    "sInfoThousands": ".",
		    "sLengthMenu": "_MENU_ resultados por página",
		    "sLoadingRecords": "Carregando...",
		    "sProcessing": "Processando...",
		    "sZeroRecords": "Nenhum registro encontrado",
		    "sSearch": "Pesquisar: ",
		    "oPaginate": {
		        "sNext": "Próximo",
		        "sPrevious": "Anterior",
		        "sFirst": "Primeiro",
		        "sLast": "Último"
		    },
		    "oAria": {
		        "sSortAscending": ": Ordenar colunas de forma ascendente",
		        "sSortDescending": ": Ordenar colunas de forma descendente"
		    }
		},
       "bProcessing": true,
       "bPaginate": true,
       "bDestroy": true,
       "sPaginationType": "full_numbers",
       "aoColumns": json.columns,
       "aaData": json.rows,
       "createdRow": function( row, data, dataIndex ) {
	    	var queryCode = $('td', row).eq(5);
	    	queryCode.replaceWith( "<td><code class='language-sql'>" 
	    		+ data[5] + "</code></td>");
		},	
		"drawCallback": function( settings ) {
			aplicarPrism(tableDom);
		},
		fnRowCallback: function( nRow, aData, iDisplayIndex, iDisplayIndexFull ) {
	    // Row click
		    $(nRow).on('click', function() {
		        var ultimo;
				console.log('modal tabela');
				if(ultimo != this){
					ultimo = this;
			        if ( $(this).hasClass('selected') ) {
			           $(this).removeClass('selected');
			        }
			        else {
			           tabela.$('tr.selected').removeClass('selected');
			           $(this).addClass('selected');
			       	}
				}
				if(selectedAct){
					selectedAct();
				}
	   		});
   		}
	});
} 