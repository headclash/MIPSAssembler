#include <print>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>

// Define os argumentos que o programa espera receber e começa a função principal.
int main(int argc, char* argv[]) {
	if (argc != 3) {
		std::println("Sintaxe Correta: ./montador <arquivo.asm> <-b|-h>"); // Informa a sintaxe correta ao usuário.                       
		return 1;
	}

	std::string input_file = argv[1];
	std::string output_format = argv[2];

	if (output_format != "-b" && output_format != "-h") {
		std::println("Formato de saída inválido. Use -b para binário ou -h para hexadecimal.");
		return 1;
	}

	return 0;
}

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