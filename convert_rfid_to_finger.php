<?php
if(isset($_GET["UID"])){
	$UID = $_GET["UID"];

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
	
	$sql = "SELECT * FROM fingerprintrfid WHERE RFIDUID='" . $UID . "'";
	$result = ($conn->query($sql));
	
	echo "!";
	
	$rows = [];
	if($result->num_rows > 0)
	{
		$rows = $result->fetch_all(MYSQLI_ASSOC);
		
		if(!empty($rows[0]))
		{
			echo $rows[0]["MtbtrID"];
		}
	}

	$conn->close();
} else {
   	echo 'Daten wurden nicht eingetragen';
}
?>