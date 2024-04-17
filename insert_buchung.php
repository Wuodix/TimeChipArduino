<?php
if(isset($_GET["Mitarbeiternummer"], $_GET["Buchungstyp"], $_GET["Zeit"])){
	$Mitarbeiternummer = $_GET["Mitarbeiternummer"];
	$Buchungstyp = $_GET["Buchungstyp"];
	$Zeit = $_GET["Zeit"];

	echo $Mitarbeiternummer;
	echo "\r\n";
	echo $Buchungstyp;
	echo "\r\n";
	echo $Zeit;
	echo "\r\n";

	$servername = "localhost";
  	$username = "Arduino";
   	$password = "pXDKlEYuYGRiRbAZ";
   	$dbname = "apotheke_time_chip";

	// Create connection
   	$conn = new mysqli($servername, $username, $password, $dbname);
   	// Check connection
   	if ($conn->connect_error) {
      		die("Connection failed: " . $conn->connect_error);
   	}
	
	$sql = "INSERT INTO buchungen_temp (MtbtrID, Buchungstyp, Zeit) VALUES ($Mitarbeiternummer,'" . $Buchungstyp . "' , '" . $Zeit . "' )";
	if ($conn->query($sql) === TRUE) {
      		echo "First query successfull";
			echo "\r\n";
   	} else {
      		echo "Error: " . $sql . " => " . $conn->error;
   	}

	$conn->close();
} else {
   	echo 'Daten wurden nicht eingetragen';
}
?>