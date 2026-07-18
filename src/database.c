#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "database.h"

// Функция для создания нового узла
static Node* create_node(const char* hostname, int port) {
    Node* new_node = (Node*)malloc(sizeof(Node));
    if (new_node == NULL) {
        perror("[database] error alocate memory.");

        exit(1);
    }
    strncpy(new_node->hostname, hostname, sizeof(new_node->hostname) - 1);
    new_node->hostname[sizeof(new_node->hostname) - 1] = '\0';  // Завершающий нулевой символ
    new_node->port = port;
    new_node->next = NULL;
    return new_node;
}

// Функция для добавления узла в конец списка
static void add_node(Node** head, const char* hostname, int port) {
    Node* new_node = create_node(hostname, port);
    if (*head == NULL) {
        *head = new_node;
    } else {
        Node* temp = *head;
        while (temp->next != NULL) {
            temp = temp->next;
        }
        temp->next = new_node;
    }
}

// Функция для загрузки данных из файла
int load_from_file(const char* filename, Node** head) {
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        perror("[database] error open file");

        return 1;
    }

    char line[512];
    while (fgets(line, sizeof(line), file)) {
        // Удаляем символ новой строки
        line[strcspn(line, "\n")] = 0;

        // Ищем разделитель ":"
        char* colon_pos = strchr(line, ':');
        if (colon_pos != NULL) {
            *colon_pos = '\0';  // Завершаем строку на месте двоеточия
            int port = atoi(colon_pos + 1);  // Преобразуем порт в целое число
            add_node(head, line, port);  // Добавляем в список
        }
    }

    fclose(file);
    return 0;
}

// Функция для вывода списка
void print_list(Node* head) {
    Node* temp = head;
    while (temp != NULL) {
        printf("Host C2: %s, Port: %d\n", temp->hostname, temp->port);
        temp = temp->next;
    }
}

int count_list(Node* head) {
    int count = 0;
    Node* temp = head;
    while (temp != NULL) {
        count += 1;
        temp = temp->next;
    }
    return count;
}

// Очистка памяти, освобождая каждый узел
void free_list(Node* head) {
    Node* temp;
    while (head != NULL) {
        temp = head;
        head = head->next;
        free(temp);
    }
}

Node* get_list_by_index(Node *head, int index_rand) {
    Node* temp = head;
    int current_index = 0;
    while (temp != NULL) {
        if (index_rand == current_index)
            return temp;
        current_index += 1;
    }
    return NULL;
}
