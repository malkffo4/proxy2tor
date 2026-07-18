#ifndef DB_H
#define DB_H

// Структура узла связанного списка
typedef struct Node {
    char hostname[256];  // Имя хоста
    int port;            // Порт
    struct Node* next;   // Указатель на следующий узел
} Node;

void free_list(Node* head);
int count_list(Node* head);
Node* get_list_by_index(Node *head, int index_rand);
int load_from_file(const char* filename, Node** head);

void print_list(Node* head);

#endif