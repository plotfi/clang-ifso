__attribute__ ((visibility ("hidden"))) int foo(int a, int b) { return a + b; }

int dataA = 34;

namespace baz {
  template <typename T>
  T add(T a, T b) {
    return a + b;
  }
}

template <typename T> T neverUsed(T t) { return t + 2; }

namespace qux {
int bar(int a, int b) { return baz::add<int>(a, b); }
}

float fbar(float a, float b) { return baz::add<float>(a, b); }

