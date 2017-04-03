SELECT ('{'
	||string_agg(
		to_json(t.key)||':'
		||(
			SELECT to_json(r)
			FROM (SELECT t.title, t.tooltip, t.description, t.help IS NOT NULL AS has_help) r
		),
	',')
	||'}')::json AS data
FROM pga_config.i18n_translations t
WHERE t.language = ${language}

