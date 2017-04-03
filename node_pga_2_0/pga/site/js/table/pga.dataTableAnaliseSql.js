function mountDataTableForSQLCommands(json, tableDom, key) {
	if (json) {
		var createdRow = function( row, data, dataIndex ) {
			var queryCode = $('td:eq(5)', row);
			queryCode.html("<code class='language-sql'>" + data[5] + "</code>");
		};
		var drawCallback = function( settings ) {
			aplicarPrism(this);
		};
		var rowClick = function(aData, nRow, iDisplayIndex, iDisplayIndexFull) {
			/* Mark the row as selected, if not already */
			if (!$(nRow).is(".selected")) {
				$(nRow).parents("table:first").find("tr").removeClass("selected");
				$(nRow).addClass("selected");
			}
			triggerSqlModal(aData[6]);
		};
		/* Hide the last column */
		json.columns[6].visible = false;
		json.columns[6].searchable = false;
		/* Build the dataTable */
		mountDataTableExhibition(json, tableDom, rowClick, null, {
			drawCallback: drawCallback,
			createdRow: createdRow
		});
	} else {
		mountEmptyTable(tableDom);
	}

}

function selectQuery(e) {
	var $el = $(e);
	if ($el.is('.opt-explain')) {
		$("#query .sql-explain").show();
		$("#query .sql-explain-analyze").hide();
		$("#query .sql-statement").show();
	} else if ($el.is('.opt-explain-analyze')) {
		$("#query .sql-explain").hide();
		$("#query .sql-explain-analyze").show();
		$("#query .sql-statement").show();
	} else {
		$("#query .sql-explain").hide();
		$("#query .sql-explain-analyze").hide();
		$("#query .sql-statement").show();
	}
	/* Select text */
	var obj = $("#query pre")[0];
	if (window.getSelection) {
		var sel = window.getSelection();
		sel.removeAllRanges();
		var range = document.createRange();
		range.selectNodeContents(obj);
		sel.addRange(range);
	} else if (document.selection) {
		var textRange = document.body.createTextRange();
		textRange.moveToElementText(obj);
		textRange.select();
	}
	return false;
}

function triggerSqlModal(code) {
	/* Remove spurious white spaces */
	/* FIXME: This currently removes spaces on string constants */
	code = code.replace(/[\r\n]/gm, ' ').replace(/\\s+/g, ' ');
	/* Add codes for EXPLAIN variants, and apply Prism */
	$("#query pre").html(
		"<code class='language-sql sql-explain' style='display:none'>"
		+ "EXPLAIN\n"
		+ "</code>"
		+ "<code class='language-sql sql-explain-analyze' style='display:none'>"
		+ "EXPLAIN (ANALYZE,VERBOSE,BUFFERS)\n"
		+ "</code>"
		+ "<code class='language-sql sql-statement'>"
		+ code
		+ "</code>"
	);
	aplicarPrism("#query pre");
	/* Prettify the query, based on Prism's processing */
	var first = true;
	var lastToken = '';
	$("#query .sql-statement .token.keyword,#query .sql-statement .token.operator,#query .sql-statement .token.comment").each(function() {
		if (!this.innerHTML.match(/as/i)) {
			var prefix = '<br/>';
			var tab = '&nbsp;&nbsp;';
			if (!$(this).is('.comment')) {
				if (this.innerHTML.match(/^(left|inner|full|join)$/i)) {
					if (!lastToken.match(/^(left|inner|full|outer)$/i)) {
						prefix = '<br/>'+tab;
					} else {
						prefix = '';
					}
				} else if (this.innerHTML.match(/^(on)$/i)) {
					prefix = '<br/>'+tab+tab;
				} else if (this.innerHTML.match(/^(distinct|into|by|all|desc|asc|nulls|first|last|status|enable|outer)$/i)) {
					prefix = '';
				} else if ($(this).is('.operator')) {
					if (this.innerHTML.match(/^(and|or)$/i)) {
						prefix = '<br/>'+tab;
					} else {
						prefix = '';
					}
				}
			}
			lastToken = this.innerHTML;
			if (!first) {
				this.innerHTML = prefix+this.innerHTML;
			}
			first = false;
		}
	});
	/* Now, just open our fancy modal */
	$('#analise-sql-modal').modal();
}

