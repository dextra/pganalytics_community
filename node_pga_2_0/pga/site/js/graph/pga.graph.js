function getValueFromType(value, type) {
	/*
	if (value === null || value === undefined) {
		return null;
	}
	*/
	switch(type) {
		case "date":
			return Date.parse(value);
		case "int":
			return parseInt(value);
		case "float":
		case "numeric":
			return Number(value);
		case "text":
		default:
			return value;
	}
}

function gerarDados(dados, keys) {
	var ret = [];
	if(dados.meta) {
		/* OBS: This loop start with 1 (instead of 0) to skip first column (X axis data) */
		for (m = 1; m < dados.meta.length; m++) {
			if (!keys || keys.indexOf(dados.meta[m].name) != -1) {
				var values = [];
				for (k = 0; k < dados.rows.length; k++) {
					/**
					 * XXX: For the X axis we are using the row position instead of the actual value. The real
					 *      value will be returned with xAxis.tickFormat function. This guarantees that is shown
					 *      observed values (present in the data set), on the other hand it gives wrong scale if
					 *      the data interval is not continuous. Although most of our data have continuous interval
					 *      and the chart that way (although not mathematically correct) seems to be easier for
					 *      our users to read.
					 */
					//values.push([getValueFromType(dados.rows[k][0], dados.meta[0].type), getValueFromType(dados.rows[k][m], dados.meta[m].type)]);
					values.push([k, getValueFromType(dados.rows[k][m], dados.meta[m].type)]);
				}
				ret.push({
					values: values,
					/* TODO: Configure in the database which series comes disabled by default (instead of hard-coded as bellow) */
					disabled: (dados.meta[m].name == "cpu_idle_perc"),
					key: i18n[dados.meta[m].name] || dados.meta[m].name
				});
			}
		}
	}
	return ret;
}

function aplicarPrism(elementDom){
	$(elementDom).find('code').each(function() {
		Prism.highlightElement(this);
	});
}

function render(svg, width, height) {
	document.createElement('canvas')
	var c = document.createElement('canvas');
	c.width = width || 500;
	c.height = height || 500;
	//document.getElementById('canvas').innerHTML = '';
	//document.getElementById('canvas').appendChild(c);
	if (typeof FlashCanvas != "undefined") {
		FlashCanvas.initElement(c);
	}
	canvg(c, svg, { log: true, renderCallback: function (dom) {
		//var a = document.createElement("a");
		//a.download = "chart.png";
		//a.href = c.toDataURL('image/png');
		//a.click();
		window.open(c.toDataURL('image/png'), '_blank');
		//c = null;
		/*
		if (typeof FlashCanvas != "undefined") {
			document.getElementById('svg').innerHTML = 'svg not supported';
		} else {
			var svg = (new XMLSerializer()).serializeToString(dom);
			document.getElementById('svg').innerHTML = svg;
			if (overrideTextBox) {
				document.getElementById('input').value = svg;
				overrideTextBox = false;
			}
		}
		//*/
	}});
}

function saveChartImage($chartsvg) {
	var svg_copy = document.createElement('svg');
	svg_copy.innerHTML = $chartsvg.html();
	$(svg_copy).find('.nv-series.disabled circle').each(function() {
		this.style.fillOpacity = 0;
	});
	$(svg_copy).find('.nv-axis line').each(function() {
		this.style.color = '#333';
		this.style.fill = 'none';
		this.style.stroke = '#ccc';
		this.style.shapeRendering = 'crispedges';
	});
	$(svg_copy).find('.nv-group circle').each(function() {
		this.style.opacity = '0.5';
		this.r = '0.5';
	});
	$(svg_copy).find('.nv-linesWrap .nv-line path').each(function() {
		this.style.fill = 'none';
		this.style.strokeWidth = '1.5px';
	});
	render('<svg>' + svg_copy.innerHTML + '</svg>', $chartsvg.width(), $chartsvg.height());
}

function quote_csv_field(data) {
	if (data.match(/[", ]/)) {
		return '"' + data.replace(/["]/g, '""') + '"';
	}
	return data;
}

function generateCSVData(data, header) {
	var csvContent = header || '';
	for (var i = 0; i < data.length; i++) {
		var row = data[i];
		for (var j = 0; j < row.length; j++) {
			if (j > 0) {
				csvContent += ',';
			}
			csvContent += quote_csv_field(row[j].toString());
		}
		csvContent += '\n';
	}
	return csvContent;
}

function saveTableCSV($tbl, $loading) {
        if (!$tbl) {
        	tbl = $('#dataTable').data('dataTableComponent');
        }  
	//var $tbl = $('#dataTable').data('dataTableComponent');
	if ($tbl) {
		var csvContent = '';
		$tbl.find('thead th').each(function() {
			if (csvContent) {
				csvContent += ',';
			}
			csvContent += quote_csv_field($(this).text());
		});
		csvContent += '\n';
		if ($tbl.DataTable().ajax.url()) {
			$loading && $loading.show();
			/* TODO: Make the server to create the CSV directly, probably faster and less memory consuming */
			$.get($tbl.DataTable().ajax.url().replace(/&format=dataTableServerSide/, ''), function(response) {
				window.open(encodeURI('data:text/csv;charset=utf-8,' + generateCSVData(response.rows, csvContent)));
				$loading && $loading.hide();
			});
		} else {
			var data = $tbl.DataTable().data();
			window.open(encodeURI('data:text/csv;charset=utf-8,' + generateCSVData(data, csvContent)));
		}
	}
}

