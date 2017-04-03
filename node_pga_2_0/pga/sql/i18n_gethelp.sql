SELECT help
FROM pga_config.i18n_translations t
WHERE t.language = ${language} AND t.key = ${key}

