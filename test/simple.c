
#include <criterion/criterion.h>

Test(prettyprint, print) {
  cr_expect(true, "value should be true");
  cr_expect(false, "value should be true");
}
