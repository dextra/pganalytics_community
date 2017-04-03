WITH mounted_devices AS (
    SELECT DISTINCT ON(sdu.fsdevice) sdu.fsdevice, sdu.mountpoint, sdu.size
    FROM sn_disk_usage sdu
    INNER JOIN sn_stat_snapshot sss ON (sss.snap_id = sdu.snap_id)
    WHERE 1=1
    AND sss.customer_id = ${customer_id}
    AND sss.server_id = ${server_id}
    AND sss.datetime >= ${date_from}::timestamptz
    AND sss.datetime <= ${date_to}::timestamptz + '1 minute'::interval
    ORDER BY sdu.fsdevice, sss.datetime DESC
)
SELECT d.dev AS device
FROM mv_sn_sysstat_disks_devs d
INNER JOIN mounted_devices md ON md.fsdevice LIKE '%'||replace(d.dev, '!', '/')||'%'
WHERE server_id = ${server_id}
GROUP BY d.dev
ORDER BY max(md.size) DESC

