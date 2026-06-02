#!/bin/bash
for i in $(find . -type f | grep -v "data/js/pdfjs" | grep -E ".png$"); do
	echo $i
	convert "$i" -strip "$i"
done
