//EXERCICIO 4: Rafael Araujo Lage e Lucas Rocha Almeida do Carmo


#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>
#include <fstream>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <vector>

using boost::asio::ip::tcp;
std::vector<std::string> valid_ids; 

const std::string ERROR_MSG = "ERROR|INVALID_SENSOR_ID\r\n";

//functions definition
void save_data(std::string msg);
std::string create_message(const std::string& msg);
bool isRegistered(std::vector<std::string> vector, std::string currentID);
void write_error_message(const std::string& message);

#pragma pack(push, 1)
struct LogRecord {
    char sensor_id[32]; // supondo um ID de sensor de até 32 caracteres
    std::time_t timestamp; // timestamp UNIX
    double value; // valor da leitura
};
#pragma pack(pop)

std::time_t string_to_time_t(const std::string& time_string) {
    std::tm tm = {};
    std::istringstream ss(time_string);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return std::mktime(&tm);
}

std::string time_t_to_string(std::time_t time) {
    std::tm* tm = std::localtime(&time);
    std::ostringstream ss;
    ss << std::put_time(tm, "%Y-%m-%dT%H:%M:%S");
    return ss.str();
}

void parseLogMessage(const std::string& mensagem, LogRecord& rec) {
    std::istringstream iss(mensagem);
    std::string token;

    std::getline(iss, token, '|'); // Ignora "LOG"
    std::getline(iss, token, '|'); // Obtem SENSOR_ID
    std::strncpy(rec.sensor_id, token.c_str(), sizeof(rec.sensor_id));

    std::getline(iss, token, '|'); // Obtem DATA_HORA
    rec.timestamp = string_to_time_t(token);

    std::getline(iss, token, '|'); // Obtem LEITURA
    rec.value = std::stod(token);
}

void parseGetMessage(const std::string& mensagem, char (&sensor_id)[32], int& num_records) {
    std::istringstream iss(mensagem);
    std::string token;

    std::getline(iss, token, '|'); // Ignora "LOG"
    std::getline(iss, token, '|'); // Obtem SENSOR_ID
    std::strncpy(sensor_id, token.c_str(), sizeof(sensor_id));

    std::getline(iss, token, '|'); // Obtem num_records
    num_records = std::stoi(token); // Usamos std::stoi para converter para inteiro
}



class session
  : public std::enable_shared_from_this<session>
{
public:
  session(tcp::socket socket)
    : socket_(std::move(socket))
  {
  }

  void start()
  {
    read_message();
  }

private:
  void read_message()
  {
    auto self(shared_from_this());
    boost::asio::async_read_until(socket_, buffer_, "\r\n",
        [this, self](boost::system::error_code ec, std::size_t length)
        {
          if (!ec)
          {
            std::istream is(&buffer_);
            std::string message(std::istreambuf_iterator<char>(is), {});
            std::cout << "Received: " << message << std::endl;
            
            std::string command = message.substr(0, 3);

            std:: cout << "Comando recebido: " << command << std::endl;
            
            LogRecord currentID;
            parseLogMessage(message, currentID);

            if(command == "LOG"){
              if(!isRegistered(valid_ids,currentID.sensor_id)){
                std::cout << currentID.sensor_id << " added" << std::endl;
                valid_ids.push_back(currentID.sensor_id);
              }
              save_data(message);
              read_message();
            } 
            else if(command == "GET"){
              if(!isRegistered(valid_ids,currentID.sensor_id)){
                std:: string error_message = ERROR_MSG;
                write_message(error_message);
              }
              else{
                write_message(message);
              }
            }

          }
        });
  }

void write_message(const std::string& message)
{
  auto self(shared_from_this());

  std::string new_message; 
  if(message == ERROR_MSG){
    new_message = ERROR_MSG;
  }
  else{
    new_message = create_message(message);
  }

  boost::asio::async_write(socket_, boost::asio::buffer(new_message),
      [this, self, new_message](boost::system::error_code ec, std::size_t /*length*/)
      {
        if (!ec)
        {
          read_message();
        }
      });
}


  tcp::socket socket_;
  boost::asio::streambuf buffer_;
};

class server
{
public:
  server(boost::asio::io_context& io_context, short port)
    : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
  {
    accept();
  }

private:
  void accept()
  {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket)
        {
          if (!ec)
          {
            std::make_shared<session>(std::move(socket))->start();
          }

          accept();
        });
  }

  tcp::acceptor acceptor_;
};

int main(int argc, char* argv[])
{
  if (argc != 2)
  {
    std::cerr << "Usage: chat_server <port>\n";
    return 1;
  }

  boost::asio::io_context io_context;

  server s(io_context, std::atoi(argv[1]));
  //server s(io_context, 9000);

  io_context.run();

  return 0;
}


//implementing functions

void save_data(std::string msg) {

    std::cout << "Criando arquivo binario" << std::endl;
    LogRecord rec;
    parseLogMessage(msg, rec);

	// Abre o arquivo para leitura e escrita em modo binário e coloca o apontador do arquivo
	// apontando para o fim de arquivo
  std:: string file_name = rec.sensor_id;
	std::fstream file(file_name + ".dat", std::fstream::out | std::fstream::in | std::fstream::binary 
																	 | std::fstream::app); 
	// Caso não ocorram erros na abertura do arquivo
	if (file.is_open())
	{
		// Imprime a posição atual do apontador do arquivo (representa o tamanho do arquivo)
		int file_size = file.tellg();

		// Recupera o número de registros presentes no arquivo
		int n = file_size/sizeof(LogRecord);

        file.seekp(0, std::ios_base::end);

		file.write((char*)&rec, sizeof(LogRecord));

		// Imprime a posição atual do apontador do arquivo (representa o tamanho do arquivo)
		file_size = file.tellg();
		// Recupera o número de registros presentes no arquivo
		n = file_size/sizeof(LogRecord);
		//std::cout << "Num records: " << n << " (file size: " << file_size << " bytes)" << std::endl;

        std :: cout << file_size << std::endl;

		// Fecha o arquivo
		file.close();
	}
	else
	{
		std::cout << "Error opening file!" << std::endl;
	}
    
}


std::string create_message(const std::string& msg){
    std::cout << "Retornando arquivo binario" << std::endl;
    int NUM_RECORDS;
    char sensor_id[32];

    parseGetMessage(msg, sensor_id, NUM_RECORDS);
    std::string records_string;

    std::string file_name = sensor_id;
    file_name += + ".dat";

    std::fstream file(file_name, std::fstream::in | std::fstream::binary);

    if (file.is_open()) {
      LogRecord rec;

      std::uintmax_t file_size = std::filesystem::file_size(file_name);
		  // Recupera o número de registros presentes no arquivo
		  int n = file_size/sizeof(LogRecord);

      //caso o arquivo seja menor que o número de registros solicitados
      if(n < NUM_RECORDS) NUM_RECORDS = n;

        for (int i = 0; i < NUM_RECORDS; i++) {
            file.read((char*)&rec, sizeof(LogRecord));
            records_string += "Sensor ID: " + std::string(rec.sensor_id) + ", Timestamp: " + time_t_to_string(rec.timestamp) + ", Value: " + std::to_string(rec.value) + "\n";
        }
    
      file.close();
    
    } else {
        std::cout << "Error opening file!" << std::endl;
    }

    return(records_string); // Atualiza a mensagem com os registros lidos
}

bool isRegistered(std::vector<std::string> vector, std::string currentID){
  bool flag = false;

    auto it = std::find(vector.begin(), vector.end(), currentID);
    
    if (it != vector.end()) {
        flag = true;
        std::cout << "Element found in vector!" << std::endl;
    } else {
      flag = false;
        std::cout << "Element not found in vector." << std::endl;
    }

  return(flag);
}