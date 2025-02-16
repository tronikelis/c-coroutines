```c
void forever() {
    long i = 69;
    while (i < 100) {
        coroutine_yield((void*)i++);
    }
}

int main() {
    Coroutine* c = coroutine_new(forever);
    while (true) {
        void* answer = coroutine_next(c);
        if (c->dead) {
            break;
        }
        long i = (long)answer;
    }

    return 0;
}
```
