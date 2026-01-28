for FILE in Makefile Documentation/images/logo.gif; do
	python3 scripts/spdxcheck.py $FILE
	python3 scripts/spdxcheck.py - < $FILE
done
python3 scripts/spdxcheck.py > /dev/null
