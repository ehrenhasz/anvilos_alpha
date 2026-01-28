cat << "END" | $@ -x c - -o /dev/null >/dev/null 2>&1
int main(void)
{
	printf("");
	return 0;
}
END
