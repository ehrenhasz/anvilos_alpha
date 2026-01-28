convert_json_to_ujson() {
  input_file=""
  output_file=".ujson" # Replace .json with .ujson
  if [ ! -f "" ]; then
    echo "Error: Input file '' not found." >&2
    return 1
  fi
  jq -c . "" > ""
  if [ 0 -eq 0 ]; then
    echo "Successfully converted '' to ''"
    rm "" # Remove the original json file
  else
    echo "Error: Failed to convert '' to ''" >&2
    return 1
  fi
}
convert_text_log_to_ujson() {
  input_file=""
  output_file=".ujson"
  if [ ! -f "" ]; then
    echo "Error: Input file '' not found." >&2
    return 1
  fi
  while IFS= read -r line; do
      echo "{\"log\": \"\"}" >> temp.ujson
  done < ""
  echo "[" > ""
  cat temp.ujson >> ""
  echo "]" >> ""
  rm temp.ujson
  rm ""
  echo "Successfully converted '' to ''"
}
if [ -f card_archive.json ]; then
  convert_json_to_ujson card_archive.json
fi
if [ -f card_queue.json ]; then
  convert_json_to_ujson card_queue.json
fi
if [ -f forge.log ]; then
  convert_text_log_to_ujson forge.log
fi
echo "Log conversion complete."
