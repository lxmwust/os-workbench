#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <stdarg.h>


#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

enum NUM_TYPE { DEC, HEX };
static const char num[] = {
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
	'a', 'b', 'c', 'd', 'e', 'f'
};

void putnum(enum NUM_TYPE type, unsigned int n) {
	int stack[20], tt = 0;
	int mod;
	
	switch (type) {
	case DEC:
		mod = 10;
		if ((int)n < 0) {
			putch('-');
			n = -(int)n;
		}
		break;
	case HEX:
		mod = 16;
		putstr("0x");
		break;
	default:
		panic("Number Type Error");
		break;
	}

	while (n) {
		stack[ ++ tt] = n % mod;
		n /= mod;
	}
	while (tt) {
		putch(num[stack[tt -- ]]);
	}
}

int printf(const char *fmt, ...) {
	va_list ap;
	char cur_fmt;
	int d;
	char c, *s;
	uintptr_t p;
	
	va_start(ap, fmt);
	while (*fmt) {
		switch (cur_fmt = *fmt++) {
		case '%':
			panic_on(!(*fmt), "Format error");
			switch (cur_fmt = *fmt++) {
			case 'c':
				c = (char)va_arg(ap, int);
				putch(c);
				break;
			case 'd':
				d = va_arg(ap, int);
				putnum(DEC, d);
				break;
			case 's':
				s = va_arg(ap, char *);
				putstr(s);
				break;
			case 'x':
				d = va_arg(ap, int);
				putnum(HEX, d);
				break;
			case 'p':
				p = va_arg(ap, uintptr_t);
				putnum(HEX, p);
				break;
			default:
				panic("No corresponding format\n");
				break;
			}
			break;
		default:
			putch(cur_fmt);
			break;
		}
	}
	va_end(ap);

	return 0;
}

int vsprintf(char *out, const char *fmt, va_list ap) {
  panic("Not implemented");
}

int sprintf(char *out, const char *fmt, ...) {
  panic("Not implemented");
}

int snprintf(char *out, size_t n, const char *fmt, ...) {
  panic("Not implemented");
}

int vsnprintf(char *out, size_t n, const char *fmt, va_list ap) {
  panic("Not implemented");
}

#endif
