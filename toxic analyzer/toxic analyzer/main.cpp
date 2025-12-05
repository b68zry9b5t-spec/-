#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <direct.h>  // 用于获取当前工作目录（可选，保留用于调试）
#include <windows.h>
#pragma warning(disable : 4996)

// 全局常量定义（避免魔法数字）
#define MAX_WORD_LENGTH 50      // 单个单词最大长度
#define MAX_STOPWORDS 1000      // 最大停用词数量
#define MAX_TOXIC_WORDS 500     // 最大有毒词数量
#define MAX_WORDS 10000         // 文本中最大唯一词数量
#define MAX_LINE_LENGTH MAX_WORD_LENGTH * 100  // 每行最大长度（统一定义，避免重复）

// 单词结构体：存储单词、频率、是否有毒、严重程度
typedef struct {
    char word[MAX_WORD_LENGTH];
    int frequency;
    int is_toxic;
    int severity;
} Word;

// 全局变量声明
char stopwords[MAX_STOPWORDS][MAX_WORD_LENGTH];   // 停用词列表
int stopword_count = 0;                           // 已加载停用词数量
char toxic_words[MAX_TOXIC_WORDS][MAX_WORD_LENGTH];// 有毒词列表
int toxic_severity[MAX_TOXIC_WORDS];              // 有毒词严重程度
int toxic_word_count = 0;                         // 已加载有毒词数量
Word word_list[MAX_WORDS];                        // 文本单词列表
int word_count = 0;                               // 总词数
int unique_word_count = 0;                        // 唯一词数

// 函数声明
void load_stopwords(const char* filename);
void load_toxic_words(const char* filename);
void normalize_word(char* word);
int is_stopword(const char* word);
int is_toxic_word(const char* word, int* severity);
void tokenize_text(const char* line);
void load_text_file(const char* filename);
void bubble_sort_words(int sort_type);
void generate_report(const char* filename);
void display_menu();

// 加载停用词（从my_stopwords.txt读取）
void load_stopwords(const char* filename) {
    FILE* file = fopen("my_stopwords.txt", "r");
    if (!file) {
        printf("警告：停用词文件my_stopwords.txt未找到，跳过停用词过滤\n");
        return;
    }

    // 修复C6385：限制读取数量，避免数组越界
    while (stopword_count < MAX_STOPWORDS &&
        fscanf(file, "%49s", stopwords[stopword_count]) == 1) {  // %49s 限制读取长度（留1字节存结束符）
        stopword_count++;
    }

    // 修复C6031：检查fclose返回值
    if (fclose(file) != 0) {
        printf("警告：停用词文件关闭失败\n");
    }
    printf("成功加载%d个停用词\n", stopword_count);
}

// 加载有毒词（从my_toxicwords.txt读取，格式：单词 严重程度）
void load_toxic_words(const char* filename) {
    FILE* file = fopen("my_toxicwords.txt", "r");
    if (!file) {
        printf("错误：有毒词文件my_toxicwords.txt未找到，程序退出\n");
        exit(1);
    }

    // 修复C6385：限制读取数量+单词长度，避免数组越界
    while (toxic_word_count < MAX_TOXIC_WORDS &&
        fscanf(file, "%49s %d", toxic_words[toxic_word_count], &toxic_severity[toxic_word_count]) == 2) {
        toxic_word_count++;
    }

    // 修复C6031：检查fclose返回值
    if (fclose(file) != 0) {
        printf("警告：有毒词文件关闭失败\n");
    }
    printf("成功加载%d个有毒词\n", toxic_word_count);
}

// 单词归一化：转为小写+去除首尾标点
void normalize_word(char* word) {
    if (word == NULL || word[0] == '\0') return;  // 增加空指针/空字符串检查

    // 转为小写
    for (int i = 0; word[i] != '\0'; i++) {
        word[i] = tolower(word[i]);
    }

    // 去除首尾标点
    int len = strlen(word);
    int start = 0, end = len - 1;
    while (start <= end && ispunct((unsigned char)word[start])) start++;
    while (end >= start && ispunct((unsigned char)word[end])) end--;

    // 修复C6385：确保截取后不越界
    if (start > end) {
        word[0] = '\0';
        return;
    }
    int valid_len = end - start + 1;
    if (valid_len > MAX_WORD_LENGTH - 1) {  // 限制有效长度不超过数组容量
        valid_len = MAX_WORD_LENGTH - 1;
    }
    memmove(word, word + start, valid_len);
    word[valid_len] = '\0';  // 手动添加结束符
}

// 判断是否为停用词（返回1是，0否）
int is_stopword(const char* word) {
    if (word == NULL || word[0] == '\0') return 0;
    for (int i = 0; i < stopword_count; i++) {
        if (strcmp(word, stopwords[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

// 判断是否为有毒词（返回1是，0否；severity存储严重程度）
int is_toxic_word(const char* word, int* severity) {
    if (word == NULL || word[0] == '\0' || severity == NULL) {
        if (severity != NULL) *severity = 0;
        return 0;
    }
    for (int i = 0; i < toxic_word_count; i++) {
        if (strcmp(word, toxic_words[i]) == 0) {
            *severity = toxic_severity[i];
            return 1;
        }
    }
    *severity = 0;
    return 0;
}

// 文本分词：将一行文本拆分为单词，存入word_list
void tokenize_text(const char* line) {
    if (line == NULL) return;

    char temp_line[MAX_LINE_LENGTH];
    // 修复C6385：strncpy + 手动添加结束符，避免缓冲区溢出
    strncpy(temp_line, line, sizeof(temp_line) - 1);
    temp_line[sizeof(temp_line) - 1] = '\0';

    char* token = strtok(temp_line, " \t\n\r");
    while (token != NULL && unique_word_count < MAX_WORDS) {
        char word[MAX_WORD_LENGTH];
        // 修复C6385：限制单词复制长度
        strncpy(word, token, MAX_WORD_LENGTH - 1);
        word[MAX_WORD_LENGTH - 1] = '\0';
        normalize_word(word);

        // 跳过空单词和停用词
        if (strlen(word) == 0 || is_stopword(word)) {
            token = strtok(NULL, " \t\n\r");
            continue;
        }

        word_count++;
        // 检查单词是否已存在
        int exists = 0;
        for (int i = 0; i < unique_word_count; i++) {
            if (strcmp(word_list[i].word, word) == 0) {
                word_list[i].frequency++;
                exists = 1;
                break;
            }
        }
        // 新单词：添加到列表
        if (!exists) {
            int severity = 0;
            int is_toxic = is_toxic_word(word, &severity);
            strcpy(word_list[unique_word_count].word, word);
            word_list[unique_word_count].frequency = 1;
            word_list[unique_word_count].is_toxic = is_toxic;
            word_list[unique_word_count].severity = severity;
            unique_word_count++;
        }
        token = strtok(NULL, " \t\n\r");
    }
}

// 加载文本文件并处理（重置全局变量，避免多次加载冲突）
void load_text_file(const char* filename) {
    if (filename == NULL || filename[0] == '\0') {
        printf("错误：文件路径为空\n");
        return;
    }

    // 重置全局变量
    word_count = 0;
    unique_word_count = 0;
    memset(word_list, 0, sizeof(word_list));

    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("错误：无法打开文件%s\n", filename);
        return;
    }

    char line[MAX_LINE_LENGTH];
    // 修复C6031：检查fgets返回值（判断是否读取成功）
    while (fgets(line, sizeof(line), file) != NULL) {
        // 去除fgets读取的换行符（避免影响分词）
        line[strcspn(line, "\n")] = '\0';
        tokenize_text(line);
    }

    // 修复C6031：检查fclose返回值
    if (fclose(file) != 0) {
        printf("警告：文本文件关闭失败\n");
    }
    printf("文件加载完成：总词数=%d，唯一词数=%d\n", word_count, unique_word_count);
}

// 冒泡排序：1=字母序（升序），2=频率（降序）
void bubble_sort_words(int sort_type) {
    if (unique_word_count <= 1) return;

    for (int i = 0; i < unique_word_count - 1; i++) {
        for (int j = 0; j < unique_word_count - i - 1; j++) {
            if (sort_type == 1) {
                // 字母序排序
                if (strcmp(word_list[j].word, word_list[j + 1].word) > 0) {
                    Word temp = word_list[j];
                    word_list[j] = word_list[j + 1];
                    word_list[j + 1] = temp;
                }
            }
            else if (sort_type == 2) {
                // 频率降序排序
                if (word_list[j].frequency < word_list[j + 1].frequency) {
                    Word temp = word_list[j];
                    word_list[j] = word_list[j + 1];
                    word_list[j + 1] = temp;
                }
            }
        }
    }
    printf("排序完成（类型：%s）\n", sort_type == 1 ? "字母序" : "频率降序");
}

// 生成分析报告并保存到文件（输出文件名为my_output.txt）
void generate_report(const char* filename) {
    FILE* file = fopen("my_output.txt", "w");
    if (!file) {
        printf("错误：无法创建报告文件my_output.txt\n");
        return;
    }

    // 计算有毒词总频率
    int total_toxic_frequency = 0;
    for (int i = 0; i < unique_word_count; i++) {
        if (word_list[i].is_toxic) {
            total_toxic_frequency += word_list[i].frequency;
        }
    }

    // 写入报告内容
    fprintf(file, "==================== 有毒文本分析报告 ====================\n");
    fprintf(file, "分析文件：%s\n", filename);
    fprintf(file, "总词数：%d\n", word_count);
    fprintf(file, "唯一词数：%d\n", unique_word_count);
    fprintf(file, "有毒词出现次数：%d\n", total_toxic_frequency);
    fprintf(file, "有毒词占比：%.2f%%\n", word_count == 0 ? 0 : (float)total_toxic_frequency / word_count * 100);

    // 写入TOP10高频单词（按频率排序）
    fprintf(file, "\n---------------------- 高频单词TOP10 ----------------------\n");
    bubble_sort_words(2);
    int top_n = unique_word_count < 10 ? unique_word_count : 10;
    for (int i = 0; i < top_n; i++) {
        const char* toxic_desc = word_list[i].is_toxic ?
            (word_list[i].severity == 1 ? "轻微有毒" :
                (word_list[i].severity == 2 ? "中等有毒" : "严重有毒")) : "正常";
        fprintf(file, "%-15s 频率：%d  类型：%s\n",
            word_list[i].word, word_list[i].frequency, toxic_desc);
    }

    // 写入所有有毒词详情
    fprintf(file, "\n---------------------- 有毒词详情 ----------------------\n");
    int toxic_count = 0;
    for (int i = 0; i < unique_word_count; i++) {
        if (word_list[i].is_toxic) {
            toxic_count++;
            fprintf(file, "单词：%-15s 频率：%d  严重程度：%d\n",
                word_list[i].word, word_list[i].frequency, word_list[i].severity);
        }
    }
    if (toxic_count == 0) {
        fprintf(file, "未检测到有毒词\n");
    }
    fprintf(file, "===========================================================\n");

    // 修复C6031：检查fclose返回值
    if (fclose(file) != 0) {
        printf("警告：报告文件关闭失败\n");
    }
    printf("报告已保存到my_output.txt\n");
}

// 菜单显示
void display_menu() {
    printf("\n==================== 有毒文本分析器 ====================\n");
    printf("1. 加载文本文件\n");
    printf("2. 显示基础统计信息\n");
    printf("3. 显示有毒词分析\n");
    printf("4. 排序并显示TOP N单词\n");
    printf("5. 保存分析报告\n");
    printf("6. 退出程序\n");
    printf("===========================================================\n");
    printf("请输入选择（1-6）：");
}

// 主函数
int main() {
    // 可选：打印当前工作目录（用于调试文件路径问题）
    char current_dir[MAX_PATH];
    if (_getcwd(current_dir, MAX_PATH) != NULL) {
        printf("程序当前工作目录：%s\n", current_dir);
    }
    else {
        printf("警告：无法获取当前工作目录\n");
    }

    // 初始化：加载停用词和有毒词词典
    load_stopwords("my_stopwords.txt");
    load_toxic_words("my_toxicwords.txt");

    int choice;
    char filename[MAX_WORD_LENGTH];
    // 提前定义switch-case中使用的变量，避免跳转跳过初始化
    int sort_type, top_n, display_n;
    int total_toxic, toxic_unique;

    while (1) {
        display_menu();
        // 处理输入，避免非法字符导致死循环
        if (scanf("%d", &choice) != 1) {
            printf("输入错误！请输入1-6之间的数字\n");
            while (getchar() != '\n');  // 清空输入缓冲区
            continue;
        }
        getchar();  // 吸收换行符（避免fgets读取空字符）

        switch (choice) {
        case 1:
            printf("请输入文本文件路径（如：input.txt）：");
            fgets(filename, MAX_WORD_LENGTH, stdin);
            filename[strcspn(filename, "\n")] = '\0';  // 去除换行符
            load_text_file(filename);
            break;
        case 2:
            total_toxic = 0;
            printf("\n=== 基础统计信息 ===\n");
            printf("总词数：%d\n", word_count);
            printf("唯一词数：%d\n", unique_word_count);
            for (int i = 0; i < unique_word_count; i++) {
                total_toxic += word_list[i].is_toxic ? word_list[i].frequency : 0;
            }
            printf("有毒词出现次数：%d\n", total_toxic);
            printf("有毒词占比：%.2f%%\n", word_count == 0 ? 0 : (float)total_toxic / word_count * 100);
            break;
        case 3:
            toxic_unique = 0;
            printf("\n=== 有毒词分析 ===\n");
            for (int i = 0; i < unique_word_count; i++) {
                if (word_list[i].is_toxic) {
                    toxic_unique++;
                    printf("单词：%-15s 频率：%d  严重程度：%d\n",
                        word_list[i].word, word_list[i].frequency, word_list[i].severity);
                }
            }
            printf("唯一有毒词数：%d\n", toxic_unique);
            if (toxic_unique == 0) {
                printf("未检测到有毒词\n");
            }
            break;
        case 4:
            printf("请选择排序类型（1=字母序，2=频率降序）：");
            if (scanf("%d", &sort_type) != 1 || (sort_type != 1 && sort_type != 2)) {
                printf("排序类型输入错误！默认按频率降序排序\n");
                sort_type = 2;
                while (getchar() != '\n');
            }
            printf("请输入显示TOP N数量：");
            if (scanf("%d", &top_n) != 1 || top_n <= 0) {
                printf("TOP N输入错误！默认显示TOP 10\n");
                top_n = 10;
                while (getchar() != '\n');
            }
            bubble_sort_words(sort_type);
            printf("\n=== TOP %d 单词 ===\n", top_n);
            display_n = unique_word_count < top_n ? unique_word_count : top_n;
            for (int i = 0; i < display_n; i++) {
                printf("%d. %-15s 频率：%d\n", i + 1, word_list[i].word, word_list[i].frequency);
            }
            break;
        case 5:
            printf("报告将保存为my_output.txt\n");
            generate_report("my_output.txt");
            break;
        case 6:
            printf("程序退出，感谢使用！\n");
            return 0;
        default:
            printf("无效选择，请输入1-6之间的数字！\n");
        }
    }
}