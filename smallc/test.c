bar (b, c)
	int b, c;
{
	int a;
	a = b + c;
	return a;
}

foo ()
{
	int d;
	void (*wibble)();
	d = bar(10, 20);
	if (d == 30) wibble();
}
