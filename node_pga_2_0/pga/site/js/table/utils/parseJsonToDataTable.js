function parseJsonToTable(data, disable_sort, attr_defs) {
	if (!data){
		return null;
	}

	var json = {'columns': []};
	$.each( data.meta, function( key, value ) {
		var coldef = {'sTitle': i18n[value.name] || value.name, 'sName': value.name};
		switch (value.type) {
			case "int":
			case "number":
			case "float":
				coldef.type = 'num';
				break;
			case "date":
				coldef.type = 'date';
				coldef.render = function(data, type, row, meta) {
					if (type !== 'display' || !data) return data;
					return i18n.formatter.fullDateTime(new Date(data));
				};
				break;
		}
		if (value.name == "alert_severity") {
			coldef.render = function(data, type, row, meta) {
				if (type == 'display') {
					return '<span class="alert-' + data + '">' + (i18n[data] || data) + '</span>';
				}
				return i18n[data] || data;
			};
		}
		if (disable_sort) {
			coldef.sortable = false;
		}
		if (attr_defs && attr_defs[value.name]) {
			var attr_def_val = attr_defs[value.name];
			console.debug(attr_def_val);
			if (attr_def_val.visible === false) {
				coldef.visible = false;
			}
			if (attr_def_val.code) {
				coldef.render = function(data, type, row, meta) {
					if (type == 'display') {
						/* The "div" is just an wrapper, it won't be present in the final output */
						var obj = $("<div><code class='language-" + attr_def_val.code + "'>" + data + "</code></div>");
						aplicarPrism(obj);
						return obj.html();
					}
					return data;
				};
			}
		}
		coldef.className = 'type-' + value.type + ' ' + 'key-' + value.name;
		coldef['bSearchable'] = (value.type === 'text');
		json['columns'].push(coldef);
	});

	json['rows'] = [];
	$.each( data.rows, function( key, value ) {
		json['rows'].push(value);
	});
	return json;
}

