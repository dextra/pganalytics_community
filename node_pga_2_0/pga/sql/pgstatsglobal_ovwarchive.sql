SELECT
	last_archived_time,
 	last_archived_wal,
	last_failed_time,
	last_failed_wal
FROM
	sn_stat_archiver
WHERE
	last_archived_time is not null
ORDER BY
	1 desc,
	2 asc,
	3 desc,
	4 asc 
