Aplicația este formată dintr-un server, un subscriber (client TCP) si un
client UDP. 

Am început prin a crea conexiunea dintre server si clientul UDP. In server
am creat un socket pentru UDP, pe care verificam dacă file descriptorul era
setat. Dacă da, se primește mesajul trimis într-un buffer si va fi trimis către 
subscriberii conectați.

Am creat clientul TCP, din care am citit si am trimis către server mesajele. 
Primul mesaj conține id-ul clientului si este trimis separat, urmând ca mesajele 
trimise ulterior sa fie de tipul subscribe/unsubscribe sau orice alt mesaj (caz 
in care este ignorat).

Am ținut un map de clienți conectați (connected_ids_fds) cărora le sunt asociați 
file descriptori. In cazul in care un client se conectează este adăugat in map și 
este afișat mesajul “new client”, dacă se afla deja in lista se afișează “already 
connected”, iar dacă nu mai primește nimic de pe socketul respectiv (a fost închis 
clientul sau a dat exit) este eliminat din map și se afișează “disconnected”.

Dacă se primește un alt mesaj in afara id-ului de client, acesta va avea la început 
dimensiunea mesajului așteptat, care va fi trunchiat sau concatenat din mesajul/mesajele 
trimise. 
Mesajul final procesat este parsat. In cazul unei comenzi de tipul “subscribe”, se adauga 
clientul cu toate datele sale într-un vector de clienți subscribed la un anumit topic. 
Dacă este de tip “unsubscribe” este eliminat din vector. In ambele cazuri este afișat 
mesajul specific. 

In final, am făcut parsarea in clientul TCP a mesajului UDP trimis către clienții 
subscribed la topicul acestuia.

Am realizat multiplexarea, afisarea mesajelor specifice corespunzătoare fiecărei 
entități ale aplicatiei (clienți, server).
