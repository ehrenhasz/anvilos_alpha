#!/bin/bash
# scripts/convert_logs.sh
# Converts existing logs to uJSON format.

# Function to convert a JSON file to uJSON
convert_json_to_ujson() {
  input_file=""
  output_file=".ujson" # Replace .json with .ujson

  if [ ! -f "" ]; then
    echo "Error: Input file '' not found." >&2
    return 1
  fi

  # Use jq to convert to ujson.
  jq -c . "" > ""

  if [ 0 -eq 0 ]; then
    echo "Successfully converted '' to ''"
    rm "" # Remove the original json file
  else
    echo "Error: Failed to convert '' to ''" >&2
    return 1
  fi
}

# Function to convert a text log file to uJSON format
convert_text_log_to_ujson() {
  input_file=""
  output_file=".ujson"

  if [ ! -f "" ]; then
    echo "Error: Input file '' not found." >&2
    return 1
  fi

  # Read each line and convert to a ujson object
  while IFS= read -r line; do
      echo "{\"log\": \"\"}" >> temp.ujson
  done < ""

  # Add brackets to make it an array for ujson validation
  echo "[" > ""
  cat temp.ujson >> ""
  echo "]" >> ""

  rm temp.ujson
  rm ""
  echo "Successfully converted '' to ''"

}

# Convert card_archive.json to ujson
if [ -f card_archive.json ]; then
  convert_json_to_ujson card_archive.json
fi

# Convert card_queue.json to ujson
if [ -f card_queue.json ]; then
  convert_json_to_ujson card_queue.json
fi

# Convert forge.log to ujson
if [ -f forge.log ]; then
  convert_text_log_to_ujson forge.log
fi

echo "Log conversion complete."
