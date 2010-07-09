<?php

// sqlinfo.hph should connect to the database and save the connection in the '$conn' variable

// possibly by including a php script in a more secure location to hide the username and password
// require("../../php/sqlinfo.php");

$dbhost = 'localhost';
$dbuser = 'username';
$dbpass = 'password';

$conn = mysql_connect($dbhost, $dbuser, $dbpass) or die('Error connecting to mysql');

$dbname = 'database';
mysql_select_db($dbname);

?>