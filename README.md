# Safe_Box
1. 개요
 - OpenSSL 라이브러리를 이용하여 파일을 암호화하여 저장하고 복호화하여 확인하는 리눅스 기반의 서버-클라이언트 모델
 - 암호화키는 패스프레이즈를 해시(MD5) 하여 사용, 암호화 알고리즘은 AES 사용

2. 사용법
 - 클라이언트 : <클라이언트 이름> <커맨드> <파일이름> <패스프레이즈>
 - 커맨드는 'get', 'put', 파일이름은 전송할 파일 이름, 패스프레이즈는 비밀번호
