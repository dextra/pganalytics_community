WITH RECURSIVE t AS (
	SELECT s.section_id, s.name, s.is_overview, 1 AS level, array[s.name] AS path
	FROM sections s
	WHERE s.parent_section IS NULL
	UNION ALL
	SELECT s.section_id, s.name, s.is_overview, t.level + 1, t.path || s.name
	FROM sections s JOIN t ON s.parent_section = t.section_id
)
SELECT t.is_overview, t.section_id, array_to_string(t.path, '.') AS section FROM t ORDER BY path, NOT is_overview;

