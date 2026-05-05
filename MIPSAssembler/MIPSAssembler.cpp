#pragma execution_character_set("utf-8")
#include <windows.h>
#include <print>
#include <string>
#include <map>
#include <fstream>
#include <sstream>

// Endereço base para o cálculo dos endereços dos rótulos, assumindo que o código começa a ser carregado a partir do
// endereço 0x00400000, que é o endereço típico para o segmento de texto em sistemas MIPS.
const int BASE_ADDRESS = 0x00400000;

// Definição da estrutura de instrução, contendo o mnemônico, tipo, opcode, funct e o padrão de operandos.
struct Instruction {
    std::string mnemonic;
    char type; // Podendo ser 'R', 'I' ou 'J'
    int opcode;
    int funct;
    std::string operand_pattern; // Varia de instrução para instrução, indicando a ordem e tipo dos operandos.
};

// Tabela contendo as instruções de cada tipo, com seus respectivos mnemônicos, tipos, opcodes, functs e padrões de 
// operandos.
Instruction instruction_table[] = {
    // Instruções R-type
    {"sll",   'R', 0,  0,  "rd,rt,shamt"},
    {"srl",   'R', 0,  2,  "rd,rt,shamt"},
    {"jr",    'R', 0,  8,  "rs"},
    {"mfhi",  'R', 0,  16, "rd"},
    {"mflo",  'R', 0,  18, "rd"},
    {"mult",  'R', 0,  24, "rs,rt"},
    {"multu", 'R', 0,  25, "rs,rt"},
    {"div",   'R', 0,  26, "rs,rt"},
    {"divu",  'R', 0,  27, "rs,rt"},
    {"add",   'R', 0,  32, "rd,rs,rt"},
    {"addu",  'R', 0,  33, "rd,rs,rt"},
    {"sub",   'R', 0,  34, "rd,rs,rt"},
    {"subu",  'R', 0,  35, "rd,rs,rt"},
    {"and",   'R', 0,  36, "rd,rs,rt"},
    {"or",    'R', 0,  37, "rd,rs,rt"},
    {"slt",   'R', 0,  42, "rd,rs,rt"},
    {"sltu",  'R', 0,  43, "rd,rs,rt"},
    {"mul",   'R', 28, 2,  "rd,rs,rt"},
    // Instruções I-type
    {"beq",   'I', 4,  0,  "rs,rt,imm"},
    {"bne",   'I', 5,  0,  "rs,rt,imm"},
    {"addi",  'I', 8,  0,  "rt,rs,imm"},
    {"addiu", 'I', 9,  0,  "rt,rs,imm"},
    {"slti",  'I', 10, 0,  "rt,rs,imm"},
    {"sltiu", 'I', 11, 0,  "rt,rs,imm"},
    {"andi",  'I', 12, 0,  "rt,rs,imm"},
    {"ori",   'I', 13, 0,  "rt,rs,imm"},
    {"lui",   'I', 15, 0,  "rt,imm"},
    {"lw",    'I', 35, 0,  "rt,imm(rs)"},
    {"sw",    'I', 43, 0,  "rt,imm(rs)"},
    // Instruções J-type
    {"j",     'J', 2,  0,  "addr"},
    {"jal",   'J', 3,  0,  "addr"}
};

// Tabela de registradores, mapeando os nomes dos registradores para seus números correspondentes.
std::map<std::string, int> register_table = {
    // Registradores escritos de forma nomeada.
    {"$zero", 0},  {"$at", 1},
    {"$v0",   2},  {"$v1", 3},
    {"$a0",   4},  {"$a1", 5},
    {"$a2",   6},  {"$a3", 7},
    {"$t0",   8},  {"$t1", 9},
    {"$t2",   10}, {"$t3", 11},
    {"$t4",   12}, {"$t5", 13},
    {"$t6",   14}, {"$t7", 15},
    {"$s0",   16}, {"$s1", 17},
    {"$s2",   18}, {"$s3", 19},
    {"$s4",   20}, {"$s5", 21},
    {"$s6",   22}, {"$s7", 23},
    {"$t8",   24}, {"$t9", 25},
    {"$k0",   26}, {"$k1", 27},
    {"$gp",   28}, {"$sp", 29},
    {"$fp",   30}, {"$ra", 31},
    // Registradores escritos de forma numérica.
    {"$0",    0},  {"$1",  1},
    {"$2",    2},  {"$3",  3},
    {"$4",    4},  {"$5",  5},
    {"$6",    6},  {"$7",  7},
    {"$8",    8},  {"$9",  9},
    {"$10",   10}, {"$11", 11},
    {"$12",   12}, {"$13", 13},
    {"$14",   14}, {"$15", 15},
    {"$16",   16}, {"$17", 17},
    {"$18",   18}, {"$19", 19},
    {"$20",   20}, {"$21", 21},
    {"$22",   22}, {"$23", 23},
    {"$24",   24}, {"$25", 25},
    {"$26",   26}, {"$27", 27},
    {"$28",   28}, {"$29", 29},
    {"$30",   30}, {"$31", 31}
};

// Função auxiliar para remover a vírgula do final de um token.
std::string strip_comma(std::string token) {
    if (!token.empty() && token.back() == ',')
        token.pop_back();
    return token;
}

// Função auxiliar para remover espaços em branco, tabulações e caracteres de nova linha do final de um token.
std::string strip_whitespace(std::string token) {
    // Remove trailing whitespace and carriage returns
    while (!token.empty() && (token.back() == ' ' ||
        token.back() == '\t' ||
        token.back() == '\r' ||
        token.back() == '\n'))
        token.pop_back();
    return token;
}

// Função auxiliar para ler um operando da linha de instrução, removendo espaços em branco e vírgulas.
std::string read_operand(std::istringstream& iss) {
    std::string operand;
    iss >> operand;
    return strip_whitespace(operand);
}

// Função impressora tipo R. Esta função recebe os componentes da instrução vindos da função read2 
void print_r(std::ofstream& output, const std::string& output_format,
    int opcode, int rs, int rt, int rd, int shamt, int funct) {

    // Constrói a instrução de 32 bits de acordo com o formato R-type.
    uint32_t instruction = 0;
    instruction |= (opcode & 0x3F) << 26;
    instruction |= (rs & 0x1F) << 21;
    instruction |= (rt & 0x1F) << 16;
    instruction |= (rd & 0x1F) << 11;
    instruction |= (shamt & 0x1F) << 6;
    instruction |= (funct & 0x3F);

    if (output_format == "-b") {
        for (int i = 31; i >= 0; i--)
            output << ((instruction >> i) & 1);
        output << "\n";
    }
    // Para o formato hexadecimal, escreve a instrução como um número hexadecimal de 8 dígitos com o auxilio da função 
    // std::format do C++20 ou superior. 
    else {
        output << std::format("{:08x}", instruction) << "\n";
    }
}

// Implementação similar à função print_r, mas adaptada para o formato I-type.
void print_i(std::ofstream& output, const std::string& output_format,
    int opcode, int rs, int rt, int imm) {

    // Constrói a instrução de 32 bits de acordo com o formato I-type.
    uint32_t instruction = 0;
    instruction |= (opcode & 0x3F) << 26;
    instruction |= (rs & 0x1F) << 21;
    instruction |= (rt & 0x1F) << 16;
    instruction |= (imm & 0xFFFF);

    // Constroi a instrução de 32 bits de acordo com o formato I-type.
    if (output_format == "-b") {
        for (int i = 31; i >= 0; i--)
            output << ((instruction >> i) & 1);
        output << "\n";
    }

    // Consultar print_r para entender a implementação do formato hexadecimal.
    else {
        output << std::format("{:08x}", instruction) << "\n";
    }
}

// Implementação similar à função print_r, mas adaptada para o formato J-type.
void print_j(std::ofstream& output, const std::string& output_format,
    int opcode, int imm) {

    uint32_t instruction = 0;
    instruction |= (opcode & 0x3F) << 26;
    instruction |= (imm & 0x3FFFFFF);
    if (output_format == "-b") {
        for (int i = 31; i >= 0; i--)
            output << ((instruction >> i) & 1);
        output << "\n";
    }

    // Consultar print_r para entender a implementação do formato hexadecimal.
    else {
        output << std::format("{:08x}", instruction) << "\n";
    }
}

// Função de leitura primaria. Esta função lê o arquivo de entrada linha por linha, remove os comentários, identifica 
// os rótulos e armazena suas posições em uma tabela de símbolos.
void read1(const std::string& filename, std::map<std::string, int>& symbol_table) {
    std::ifstream file(filename);
    int line_number = 0;
    std::string line;

    while (std::getline(file, line)) {
        // Remove comentários presentes na linha
        size_t comment_pos = line.find('#');
        if (comment_pos != std::string::npos)
            line = line.substr(0, comment_pos);

        // Pula linhas vazias
        if (line.empty()) continue;

        // Verifica se a linha possui um label, encontrando a posição do caractere ':' e armazenando o nome do label e 
        // sua posição na tabela de símbolos.
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string label = line.substr(0, colon_pos);
            symbol_table[label] = line_number + 1;
        }

        line_number++;
    }
}

// Função de leitura secundária. Esta função lê o arquivo novamente, remove os comentários e rótulos, e processa as
// instruções, convertendo-as para o formato binário ou hexadecimal conforme especificado pelo usuário. 
// Além disso, também conta a quantidade de cada tipo de instrução para fins de relatório.
void read2(const std::string& filename, const std::string& output_format, const std::map<std::string,
    int>& symbol_table, std::map<std::string, int>& instruction_count) {

    // Abre o arquivo de entrada e prepara o arquivo de saída, definindo o nome do arquivo de saída com base no nome do
    // arquivo de entrada e no formato de saída escolhido pelo usuário no começo do programa.
    std::ifstream file(filename);
    std::string output_filename = filename.substr(0, filename.find('.'));
    output_filename += (output_format == "-b") ? ".bin" : ".hex";

    std::ofstream output(output_filename);

    if (output_format == "-h") // Escreve o cabeçalho para o formato hexadecimal.
        output << "v2.0 raw\n";

    int line_number = 0;
    std::string line;

    while (std::getline(file, line)) {
        // Remove cos comentários presentes na linha
        size_t comment_pos = line.find('#');
        if (comment_pos != std::string::npos)
            line = line.substr(0, comment_pos);

        // Encontra os labels como na função anterior, mas desta vez remove os rótulos da linha para processar apenas 
        // a instrução.
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos)
            line = line.substr(colon_pos + 1);

        // Pula linhas vazias e remove espaços em branco no início da linha para facilitar a identificação do 
        // mnemônico.
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        line = line.substr(start);

        line_number++;

        // Prcessa o mnemonico da instrução.
        std::istringstream iss(line);
        std::string mnemonic;
        iss >> mnemonic;

        // Procura a instrução correspondente na tabela de instruções.
        Instruction* instr = nullptr;
        for (auto& entry : instruction_table) {
            if (entry.mnemonic == mnemonic) {
                instr = &entry;
                break;
            }
        }

        // Se a instrução não for encontrada, exibe uma mensagem de erro e continua para a próxima linha.
        if (instr == nullptr) {
            std::println("Instrução desconhecida: {}", mnemonic);
            continue;
        }

        // Incrementa a contagem da instrução processada para fins de relatório.
        instruction_count[mnemonic]++;

        // Instruções de como o montador deve processar os operandos de cada instrução, dependendo do padrão de 
        // operandos definido na tabela de instruções.
        int rs = 0, rt = 0, rd = 0, shamt = 0, imm = 0;
        std::string operand;

        if (instr->operand_pattern == "rd,rs,rt") {
            operand = read_operand(iss); rd = register_table[strip_comma(operand)];
            operand = read_operand(iss); rs = register_table[strip_comma(operand)];
            operand = read_operand(iss); rt = register_table[operand];
        }

        else if (instr->operand_pattern == "rd,rt,shamt") {
            operand = read_operand(iss); rd = register_table[strip_comma(operand)];
            operand = read_operand(iss); rt = register_table[strip_comma(operand)];
            operand = read_operand(iss); shamt = std::stoi(operand);
        }

        else if (instr->operand_pattern == "rs,rt") {
            operand = read_operand(iss); rs = register_table[strip_comma(operand)];
            operand = read_operand(iss); rt = register_table[operand];
        }

        else if (instr->operand_pattern == "rs") {
            operand = read_operand(iss); rs = register_table[operand];
        }

        else if (instr->operand_pattern == "rd") {
            operand = read_operand(iss); rd = register_table[operand];
        }

        else if (instr->operand_pattern == "rt,imm(rs)") {
            operand = read_operand(iss); rt = register_table[strip_comma(operand)];
            operand = read_operand(iss);

            // Caso especial para quanto (x) for separado do registrador por um espaço.
            if (operand.find('(') == std::string::npos) {
                imm = std::stoi(operand);
                operand = read_operand(iss); // Leia (x) como o próximo token.
            }
            else {
                imm = std::stoi(operand.substr(0, operand.find('(')));
            }

            size_t open_paren = operand.find('(');
            size_t close_paren = operand.find(')');
            std::string reg = operand.substr(open_paren + 1, close_paren - open_paren - 1);
            rs = register_table[reg];
        }

        else if (instr->operand_pattern == "rs,rt,imm") {
            operand = read_operand(iss); rs = register_table[strip_comma(operand)];
            operand = read_operand(iss); rt = register_table[strip_comma(operand)];
            operand = read_operand(iss);
            operand = strip_comma(operand);

            // Se o operando for um rótulo, calcula o deslocamento relativo para instruções de desvio ou o endereço
            // absoluto para instruções de salto, utilizando a tabela de símbolos para encontrar a linha correspondente
            // ao rótulo e convertendo essa linha para um endereço de memória.
            auto symIt = symbol_table.find(operand);
            if (symIt != symbol_table.end()) {
                if (mnemonic == "beq" || mnemonic == "bne") {
                    int label_line = symIt->second;
                    imm = label_line - (line_number + 1); // Offset relativo
                }
                else {
                    int label_line = symIt->second;
                    int label_address = BASE_ADDRESS + (label_line - 1) * 4;
                    imm = label_address / 4; // Ajusta para o formato de endereço usado em instruções J-type
                }
            }
            else {
                // Processa o operando como um valor imediato, convertendo a string para um inteiro e lidando com
                // possíveis erros de conversão, como argumentos inválidos ou valores fora do intervalo permitido.
                try {
                    imm = std::stoi(operand);
                }
                catch (const std::invalid_argument&) {
                    std::println("Line {}: invalid immediate '{}'", line_number, operand);
                    continue;
                }
                catch (const std::out_of_range&) {
                    std::println("Line {}: immediate out of range '{}'", line_number, operand);
                    continue;
                }
            }
        }

        else if (instr->operand_pattern == "rt,rs,imm") {
            operand = read_operand(iss); rt = register_table[strip_comma(operand)];
            operand = read_operand(iss); rs = register_table[strip_comma(operand)];
            operand = read_operand(iss);
            operand = strip_comma(operand);

            try {
                imm = std::stoi(operand);
            }
            catch (const std::invalid_argument&) {
                std::println("Linha {}: imediato inválido '{}'", line_number, operand);
                continue;
            }
            catch (const std::out_of_range&) {
                std::println("Linha {}: imediato fora do intervalo '{}'", line_number, operand);
                continue;
            }
        }

        else if (instr->operand_pattern == "rt,imm") {
            operand = read_operand(iss); rt = register_table[strip_comma(operand)];
            operand = read_operand(iss); imm = std::stoi(operand);

        }

        else if (instr->operand_pattern == "addr") {
            operand = read_operand(iss);
            if (symbol_table.find(operand) != symbol_table.end()) {
                int label_line = symbol_table.at(operand);
                int label_address = BASE_ADDRESS + (label_line - 1) * 4;
                imm = label_address / 4;
            }
            else {
                std::println("Rótulo desconhecido: {}", operand);
                continue;
            }
        }

        // Após processar os operandos, chama a função de impressão correspondente ao tipo da instrução para converter
        // a instrução para o formato binário ou hexadecimal e escrevê-la no arquivo de saída.
        if (instr->type == 'R')
            print_r(output, output_format, instr->opcode, rs, rt, rd, shamt, instr->funct);
        else if (instr->type == 'I')
            print_i(output, output_format, instr->opcode, rs, rt, imm);
        else if (instr->type == 'J')
            print_j(output, output_format, instr->opcode, imm);

    }

}

int main(int argc, char* argv[]) {
    SetConsoleOutputCP(CP_UTF8);

    if (argc != 3) {
        std::println("Sintaxe Correta: ./montador <arquivo.asm> <-b|-h>"); // Informa a sintaxe correta ao usuário.                       
        return 1;
    }

    // Lê os argumentos de linha de comando, onde o primeiro argumento é o nome do arquivo de entrada e o segundo 
    // argumento indica o formato de saída desejado pelo usuário.
    std::string input_file = argv[1];
    std::string output_format = argv[2];

    // Declara a tabela de símbolos para armazenar os rótulos e suas posições.
    std::map<std::string, int> symbol_table;
    // Declara a tabela de contagem de instruções para armazenar a quantidade de cada tipo de instrução processada.
    std::map<std::string, int> instruction_count;

    read1(input_file, symbol_table);
    //for (auto& [label, line] : symbol_table)
    //    std::println("Label: {} -> Linha: {}", label, line);
    read2(input_file, output_format, symbol_table, instruction_count);

    // Declara a tabela de ciclos para armazenar o número de ciclos de cada tipo de instrução, lendo os dados do 
    // arquivo CSV "ciclos.csv" e armazenando-os em um mapa para fácil acesso durante o cálculo do CPI médio.
    std::map<std::string, int> cycles_table;

    std::ifstream csv("ciclos.csv");
    std::string csv_line;
    std::getline(csv, csv_line); // Pula o cabeçalho do arquivo CSV.

    // Lê cada linha do arquivo CSV, separa o nome da instrução e o número de ciclos, e armazena essas informações na 
    // tabela de ciclos para uso posterior no cálculo do CPI médio.
    while (std::getline(csv, csv_line)) {
        std::istringstream iss(csv_line);
        std::string instr_name;
        std::string cycles;
        std::getline(iss, instr_name, ',');
        std::getline(iss, cycles, ',');
    //    std::println("Lendo: '{}' = '{}'", instr_name, cycles);
        cycles_table[instr_name] = std::stoi(cycles);
    }

    // Imprime a quantidade de instruções processadas por tipo.
    std::println("Quantidades por tipo de instruções:");
    for (auto& [name, count] : instruction_count)
        std::println("{}: {}", name, count);

    // Calcula o CPI médio utilizando a tabela de ciclos e a contagem de instruções.
    int total_instructions = 0;
    double total_cycles = 0;

    for (auto& [name, count] : instruction_count) {
        total_instructions += count;
        if (cycles_table.find(name) != cycles_table.end())
            total_cycles += count * cycles_table[name];
    }

    double average_cpi = total_cycles / total_instructions;
    std::println("\nCPI médio: {:.2f}", average_cpi);

    // Lida com possíveis erros de formato de saída, informando o usuário sobre os formatos válidos e encerrando o 
    // programa caso um formato inválido seja fornecido.
    if (output_format != "-b" && output_format != "-h") {
        std::println("Formato de saída inválido. Use -b para binário ou -h para hexadecimal.");
        return 1;
    }

    return 0;
}
