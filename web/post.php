<?php
print("start<br />");
$up_file  = "";
$up_ok = false;
$tmp_file = isset($_FILES["upfile"]["tmp_name"]) ? $_FILES["upfile"]["tmp_name"] : "";
print($tmp_file. " < tmp file name<br />");
$org_file = isset($_FILES["upfile"]["name"])     ? $_FILES["upfile"]["name"]     : "";
print($org_file. " < original file name<br />");

if( $tmp_file != "" && is_uploaded_file($tmp_file) ){
 $split = explode('.', $org_file); 
 $ext = end($split);
 if( $ext != "" && $ext != $org_file  ){
   $up_file = "shot.jpg";
   $up_ok = move_uploaded_file( $tmp_file, $up_file);
   print("ok<br />");
 }
}

print("end");

?>
