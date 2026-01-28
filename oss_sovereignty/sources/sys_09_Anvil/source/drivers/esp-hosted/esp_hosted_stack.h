

#ifndef MICROPY_INCLUDED_DRIVERS_ESP_HOSTED_STACK_H
#define MICROPY_INCLUDED_DRIVERS_ESP_HOSTED_STACK_H

#include <stdbool.h>
#include <stddef.h>
#include "esp_hosted_internal.h"

static inline void esp_hosted_stack_init(esp_hosted_stack_t *stack) {
    stack->capacity = ESP_STACK_CAPACITY;
    stack->top = 0;
}

static inline bool esp_hosted_stack_empty(esp_hosted_stack_t *stack) {
    return stack->top == 0;
}

static inline bool esp_hosted_stack_push(esp_hosted_stack_t *stack, void *data) {
    if (stack->top < stack->capacity) {
        stack->buff[stack->top++] = data;
        return true;
    }
    return false;
}

static inline void *esp_hosted_stack_pop(esp_hosted_stack_t *stack, bool peek) {
    void *data = NULL;
    if (stack->top > 0) {
        data = stack->buff[stack->top - 1];
        if (!peek) {
            stack->top -= 1;
        }
    }
    return data;
}
#endif 
