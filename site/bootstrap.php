<?php

require("sqlinfo.php");

$reg = ($_REQUEST['reg'] == '1');
$dbg = ($_REQUEST['dbg'] == '1');
$port = (int)($_REQUEST['port']);
$type = preg_replace("/[^a-zA-Z0-9]/", "", $_REQUEST['type']);
$class = preg_replace("/[^a-zA-Z0-9]/", "", $_REQUEST['class']);
if ($port === 0) $port = 47189;

if ($reg) {
	$query = 'INSERT INTO `peers` (' .
	'`id` ,' .
	'`address` ,' .
	'`port` ,' .
	'`time` ,' .
	'`type` ,' .
	'`class`' .
	')' .
	'VALUES (' .
	"NULL , '" . $_SERVER['REMOTE_ADDR'] . "', '" . $port ."', NOW( ), '" . $type . "', '" . $class . "'" .
	');';
	$result = mysql_query($query);
}

$query = 'SELECT NOW() as time';
$result = mysql_fetch_array(mysql_query($query), MYSQL_ASSOC);
$sqlnow = $result['time'];
$phpnow = strtotime( $sqlnow );

$sqlt4 = date( 'Y-m-d H:i:s', $phpnow - (4*60*60) );
$sqlt1 = date( 'Y-m-d H:i:s', $phpnow - (1*60*60) );

if (true) {
	// delete old records
	$query = "DELETE FROM peers WHERE (time < '" . $sqlt4 . "')";
	$result = mysql_query($query);
}

if ($dbg)
	//$query = 'SELECT * FROM peers';
	$query = "SELECT * FROM peers ";
else
	$query = "SELECT DISTINCT address,port FROM peers ";
$query .= "WHERE (time > '" . $sqlt1 . "') AND (type = '" . $type . "') AND (class = '" . $class . "')";
$result = mysql_query($query);

echo '*S*T*A*R*T*';
echo chr(13);

while ($row = mysql_fetch_array($result, MYSQL_ASSOC))
{
	//echo expand($row);
	if ($dbg) echo $row['id'] . chr(9);
	echo $row['address'];
	echo chr(9);
	echo $row['port'];
	if ($dbg) echo chr(9) . $row['time'];
	if ($dbg) echo chr(9) . $row['type'];
	if ($dbg) echo chr(9) . $row['class'];

	echo chr(13);
}

echo '*S*T*O*P*';
echo chr(13);

mysql_close($conn);
?>
