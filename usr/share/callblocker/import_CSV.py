#!/usr/bin/env python

# callblocker - blocking unwanted calls from your home phone
# Copyright (C) 2015-2015 Patrick Ammann <pammann@gmx.net>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 3
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#

from __future__ import print_function
import os, sys, argparse, re
import codecs, csv
from collections import OrderedDict
from datetime import datetime
import json


g_debug = False


def error(*objs):
  print("ERROR: ", *objs, file=sys.stderr)
  sys.exit(-1)

def debug(*objs):
  if g_debug: print("DEBUG: ", *objs, file=sys.stdout)
  return

def find_delimiter(filname):
  with open(filname, 'r') as f:
    line = f.readline()
    semi_cnt = line.count(";")
    comma_cnt = line.count(",")
    delimiter = ","
    if semi_cnt >  comma_cnt: delimiter = ";"
    debug("Correct delimiter is %s" % delimiter)
    return delimiter

class UTF8Recoder:
    """
    Iterator that reads an encoded stream and reencodes the input to UTF-8
    """
    def __init__(self, f, encoding):
        self.reader = codecs.getreader(encoding)(f)

    def __iter__(self):
        return self

    def next(self):
        return self.reader.next().encode("utf-8")

class UnicodeDictReader:
    """
    A CSV reader which will iterate over lines in the CSV file "f",
    which is encoded in the given encoding.
    """
    def __init__(self, f, delimiter=',', dialect=csv.excel, encoding="utf-8"):
        f = UTF8Recoder(f, encoding)
        self.reader = csv.DictReader(f, delimiter=delimiter, dialect=dialect)

    def next(self):
        row = self.reader.next()
        for key in row:
            if row[key] != None:
              try:
                row[key] = row[key].decode("utf-8")
              # Special case, sometimes the content gets reqd as a list
              except AttributeError:
                  newList = []
                  for item in row[key]:
                      newList.append(item.decode("utf-8"))
                  row[key] = newList
            else:
              row[key] = ''
        return row

    def __iter__(self):
        return self


# Finding the correct encoding for the file
def find_encoding(filname, delimiter):
  debug("Detecting encoding of the CSV file...")

  all_encoding = [
    "utf-8", "iso-8859-1", "iso-8859-2", "iso-8859-15",
    "iso-8859-3", "us-ascii", "windows-1250", "windows-1252",
    "windows-1254", "ibm861"
  ]
  encoding_index = 0
  csv_reader = None
  while csv_reader == None:  
    next_encoding = all_encoding[encoding_index]
    debug("Trying %s" % (next_encoding))
    csv_file = open(filname, "rt")
    csv_reader = UnicodeDictReader(csv_file, delimiter=delimiter, encoding=next_encoding)
    try:
      for line in enumerate(csv_reader):
        # Do nothing, just reading the whole file
        encoding_index = encoding_index
    except UnicodeDecodeError:
      csv_reader = None
      input_csv_file.close()
      encoding_index = encoding_index + 1

  debug("Correct encoding is %s" % next_encoding)
  csv_file.close()
  return next_encoding

def extract_number(data):
  n = re.sub(r"[^0-9\+]","", data)
  return n

def getEntityPerson(fields):
  name = ""
  # first name
  for field_name in fields:
    if field_name and field_name.lower().find("first name") != -1:
      name += " " + fields[field_name]
      break
  # middle name
  for field_name in fields:
    if field_name and field_name.lower().find("middle name") != -1:
      name += " " + fields[field_name]
      break
  # last name
  for field_name in fields:
    if field_name and field_name.lower().find("last name") != -1:
      name += " " + fields[field_name]
      break
  # tellows: name
  if "Score" in fields and "Anruftyp" in fields:
    name = "%s / score:%s" % (fields["Anruftyp"], fields["Score"])
  return name.strip()

def parse_csv(filename, delimiter, encoding, result):
  csv_file = open(filename, "rt")
  csv_reader = UnicodeDictReader(csv_file, delimiter=delimiter, encoding=encoding)

  date = datetime.utcnow().strftime("%Y-%m-%d %H:%M:%S +0000")
  #debug(date)

  for (line, fields) in enumerate(csv_reader):
    #debug(fields)
    name = getEntityPerson(fields)
    #debug(name)
    for field_name in fields:
      if not field_name: continue
      number = ""
      if field_name.lower().find("phone") != -1:
        number = fields[field_name]
      elif field_name.lower().find("pager") != -1:
        number = fields[field_name]
      elif field_name.lower().find("fax") != -1:
        number = fields[field_name]
      # tellows: number = Land Nummer
      elif field_name.find("Nummer") != -1 and "Land" in fields:
        number = "+%s%s" % (fields["Land"], fields[field_name][1:])
        field_name = "phone"

      number = extract_number(number)
      if len(number) != 0:
        result.append({"number":number, "name":name+" ("+field_name+")", "date_created":date, "date_modified":date})

  csv_file.close()
  return result  

# remove duplicates
# remove too small numbers -> dangerous
# make sure numbers are in international format (e.g. +41AAAABBBBBB)
def cleanup_entries(arr, country_code):
  debug("cleanup_entries...")
  seen = set()
  uniq = []
  for r in arr:
    x = r["number"]

    # make international format
    if x.startswith("00"):  x = "+"+x[2:]
    elif x.startswith("0"): x = country_code+x[1:]
    r["number"] = x

    # filter
    if len(x) < 4:
      # too dangerous
      debug("Skip too small number: " + str(r))
      continue
    if not x.startswith("+"):
      # not in international format
      debug("Skip unknown format number: " + str(r))
      continue;
    if len(x) > 16:
      # see spec E.164 for international numbers: 15 (including country code) + 1 ("+")
      debug("Skip too long number:" + str(r))
      continue;

    if x not in seen:
      uniq.append(r)
      seen.add(x)

  debug("cleanup_entries done")
  return uniq

#
# main
#
def main(argv):
  global g_debug
  parser = argparse.ArgumentParser(description="Convert CSV file to json")
  parser.add_argument("--input", help="input file", required=True)
  parser.add_argument("--country_code", help="country code, e.g. +41", required=True)
  parser.add_argument("--merge", help="file to merge with", default="out.json")
  parser.add_argument('--debug', action='store_true')
  args = parser.parse_args()
  g_debug = args.debug

  name = os.path.splitext(os.path.basename(args.merge))[0]
  result = []

  # merge
  try:
    data = open(args.merge, "r").read()
    j = json.loads(data)
    name = j["name"]
    result = j["entries"]
    debug(result)
  except IOError:
    pass

  # convert
  delimiter = find_delimiter(args.input)
  encoding = find_encoding(args.input, delimiter)
  result = parse_csv(args.input, delimiter, encoding, result)

  result = cleanup_entries(result, args.country_code)
  print(result)
  if len(result) != 0:
    data = OrderedDict((
      ("name", name),
      ("entries", result)
    ))
    with open(args.merge, 'w') as outfile:
      json.dump(data, outfile, indent=2)

if __name__ == "__main__":
    main(sys.argv)
    sys.exit(0)

