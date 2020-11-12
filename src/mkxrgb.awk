BEGIN {
	FS = " ";
	print "struct xcolor colors[]={"
}

END { print "};" }

{
	printf "\t{\"%s\",%s,%s,%s},\n",$4,$1,$2,$3;
}

