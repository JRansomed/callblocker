<?php
/*
 callblocker - blocking unwanted calls from your home phone
 Copyright (C) 2015-2015 Patrick Ammann <pammann@gmx.net>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 3
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

  include("settings.php");

  # dojo json exchange format see:
  # http://dojotoolkit.org/reference-guide/1.10/dojo/data/ItemFileReadStore.html#input-data-format

/*
  function scanListFiles($dirName) {
    $files = scandir(CALLBLOCKER_SYSCONFDIR."/".$dirName);
    $ret = array();
    foreach($files as $f) {
      $file = CALLBLOCKER_SYSCONFDIR."/".$dirName."/".$f;
      if (pathinfo($file)["extension"] == "json") {
        array_push($ret, $file);
      }
    }
    return $ret;
  }
  function getList($dirName, $id) {
    $files = scanListFiles($dirName);
    if ($id >= count($files)) return array();
    $json = json_decode(file_get_contents($files[$id]));
    //var_dump($json);
    return $json->{"entries"};
  }
  $all = getList("blacklists", 0);
*/

  $dirname = "blacklists";
  if (array_key_exists("dirname", $_REQUEST)) {
    $dirname = $_REQUEST["dirname"];
  }
  $file = CALLBLOCKER_SYSCONFDIR."/".$dirname."/main.json";
  //print $file;
  
  if (array_key_exists("data", $_POST)) {
    //error_log("POST data:".$_POST["data"]);
    $json = json_decode($_POST["data"]);
    $ret = array("name"=>$json->{"label"},
                 "entries"=>$json->{"items"});
    file_put_contents($file, json_encode($ret, JSON_PRETTY_PRINT));
    return;
  }

  $json = json_decode(file_get_contents($file));
  #var_dump($json);
  $all = $json->{"entries"};
  #var_dump($all);
  $all_count = count($all);

  $start = 0;
  $count = $all_count;

  // Handle paging, if given.
  if (array_key_exists("start", $_REQUEST)) {
    $start = $_REQUEST["start"];
  }
  if (array_key_exists("count", $_REQUEST)) {
    $count = $_REQUEST["count"];
  }

  $ret = array();
  for ($i = $start; $i < $start+$count && $i < $all_count; $i++) {
    $entry = $all[$i];
    array_push($ret, $entry);
  }

  header("Content-Type", "text/json");
  header(sprintf("Content-Range: items %d-%d/%d", $start, $start+$count, $all_count));
  print json_encode(array("numRows"=>$all_count, "label"=>"MY Label Test 123", "items"=>$ret), JSON_PRETTY_PRINT);
?>
