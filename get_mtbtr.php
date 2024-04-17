<?php
if(isset($_GET["Mitarbeiternummer"])){
	$Mitarbeiternummer = $_GET["Mitarbeiternummer"];

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
	
	$sql = "SELECT * FROM mitarbeiter WHERE ID=$Mitarbeiternummer";
	$result = ($conn->query($sql));
	
	$sql2 = "SELECT * FROM buchungen_temp WHERE Buchungsnummer=(SELECT max(Buchungsnummer) FROM buchungen_temp WHERE MtbtrID=$Mitarbeiternummer)";
	$result2 = ($conn->query($sql2));
	
	echo "!";
	
	$row = [];
	$row2 = [];
	if($result->num_rows > 0)
	{
		$row = $result->fetch_all(MYSQLI_ASSOC);
		$row2 = $result2->fetch_all(MYSQLI_ASSOC);
		
		if(!empty($row))
		{
			foreach($row as $rows)
			{
				if(!empty($row2))
				{
					foreach($row2 as $rows2)
					{
						echo $rows["Vorname"];
						echo '$';
						echo $rows["Nachname"];
						echo '$';
						echo $rows2["Buchungstyp"];
						echo '$';
					}
				}
				else
				{
					echo $rows["Vorname"];
					echo '$';
					echo $rows["Nachname"];
					echo'$$';
				}
			}
		}
	}
	else
	{
		echo "NoMTBTR";
	}

	$conn->close();
} else {
   	echo 'Daten wurden nicht eingetragen';
}
?>