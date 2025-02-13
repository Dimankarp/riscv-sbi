extern char __bss[], __stack_top[];

typedef struct {
  long error;
  long value;
} sbiret;

sbiret sbi_call(long arg0, long arg1, long arg2, long arg3, long arg4,
                long arg5, long fid, long eid) {
  register long a0 __asm__("a0") = arg0;
  register long a1 __asm__("a1") = arg1;
  register long a2 __asm__("a2") = arg2;
  register long a3 __asm__("a3") = arg3;
  register long a4 __asm__("a4") = arg4;
  register long a5 __asm__("a5") = arg5;
  register long a6 __asm__("a6") = fid;
  register long a7 __asm__("a7") = eid;

  __asm__ __volatile__("ecall"
                       : "=r"(a0), "=r"(a1)
                       : "r"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5),
                         "r"(a6), "r"(a7)
                       : "memory");
  return (sbiret){.error = a0, .value = a1};
}

long put_char(char ch) { return sbi_call(ch, 0, 0, 0, 0, 0, 0, 1).error; }
long get_char() { return sbi_call(0, 0, 0, 0, 0, 0, 0, 2).error; }

#define SBI_SPEC_MINOR 0x00FFFFFF
#define SBI_SPEC_MAJOR 0x7F000000
#define GET_MINOR(val) (val) & SBI_SPEC_MINOR
#define GET_MAJOR(val) ((val) & SBI_SPEC_MAJOR) >> 24

#define GET_CSR(val) ((val) & 0x00000FFF)
#define GET_WIDTH(val) ((val) & 0x0003F000) >> 12
#define GET_TYPE(val) ((val) & 0x10000000) >> 31

sbiret get_spec_ver() { return sbi_call(0, 0, 0, 0, 0, 0, 0, 0x10l); }
sbiret get_counters_num() { return sbi_call(0, 0, 0, 0, 0, 0, 0, 0x504d55l); }
sbiret get_counter_info(unsigned long idx) {
  return sbi_call(idx, 0, 0, 0, 0, 0, 1, 0x504d55l);
}

void shutdown() { sbi_call(0, 0, 0, 0, 0, 0, 0, 0x08l); }

void memset(char *buf, long len, char c) {
  for (int i = 0; i < len; i++)
    buf[i] = c;
}

void print_str(char *str) {
  char *cur = str;
  while (*cur) {
    put_char(*cur);
    cur++;
  }
}

void print_long(long l) {
  char buf[255];
  memset(buf, 255, '\0');
  char *cur = buf + 254;
  if (l == 0) {
    put_char('0');
  }
  while (l > 0) {
    cur--;
    *cur = l % 10 + '0';
    l /= 10;
  }
  print_str(cur);
}

int read_line(char *buf, long len) {
  int count = 0;
  for (int i = 0; i < len - 1; i++) {
    for (;;) {
      long c = get_char();
      if (c != -1) {
        if (c == '\r')
          c = '\n';
        put_char(c);
        if (c == '\0' || c == '\n')
          return count;
        *buf = c;
        buf++;
        count++;
      }
    }
  }
  return count;
}

long strtol(char *str) {
  long res = 0;
  while (*str) {
    if (*str < '0' || *str > '9')
      return -1;
    res *= 10;
    res += *str - '0';
    str++;
  }
  return res;
}

char read_option(int max) {}

void print_sbi_version() {
  sbiret val = get_spec_ver();
  long major = GET_MAJOR(val.value);
  long minor = GET_MINOR(val.value);

  print_long(major);
  print_str(".");
  print_long(minor);
  print_str("v\n");
}

void print_counters_num() {
  sbiret val = get_counters_num();
  print_long(val.value);
  print_str("\n");
}

void print_counter_info(long idx) {
  sbiret val = get_counter_info(idx);
  long csr = GET_CSR(val.value);
  long width = GET_WIDTH(val.value);
  long type = GET_TYPE(val.value);
  print_str("CSR: ");
  print_long(csr);
  print_str("\n");
  print_str("Width: ");
  print_long(width);
  print_str("\n");
  print_str("Type: ");
  if (type == 0)
    print_str("hardware\n");
  else
    print_str("firmware\n");
  print_str("\n");
}

void kernel_main(void) {
  print_str("Menu:\n");
  print_str(" 1. Get SBI specification number.\n");
  print_str(" 2. Get number of counters.\n");
  print_str(" 3. Get details of a counter by number.\n");
  print_str(" 4. System Shutdown.\n");

  char buf[255];
  memset(buf, 255, '\0');
  read_line(buf, 10);
  int status = strtol(buf);
  if (status < 0) {
    print_str("Failed to parse option!\n");
    return;
  }

  switch (status) {
  case 1: {
    print_sbi_version();
    break;
  }
  case 2: {
    print_counters_num();
    break;
  }
  case 3: {
    memset(buf, 255, '\0');
    print_str("Write counter number:");
    read_line(buf, 10);
    int status = strtol(buf);
    if (status > 0)
      print_counter_info(status);
    else
      print_str("Failed to parse counter index\n");
    break;
  }
  case 4: {
    shutdown();
    break;
  }
  default: {
    return;
  }
  }
}

__attribute__((section(".text.boot"))) __attribute__((naked)) void boot(void) {
  __asm__ __volatile__("mv sp, %[stack_top]\n"
                       "j kernel_main\n"
                       :
                       : [stack_top] "r"(__stack_top));
}