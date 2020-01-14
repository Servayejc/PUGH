Hardware
	- Le plan de montage est dans PUGH.jpg
	- Pour le montage final, R4 et la LED ne sont pas nécessaires.
	- D6 fournit des impulsions pour simuler le débimètre la led clignote.
	- Pour tester sans le débimètre, enlever le fil bleu de la borne D5 et relier D5 et D6
	- Alimenter le module via avec une petite alimentation USB (5V), comme pour le programmer.
	- Il peut aussi être alimenté avec 5 à 7 V DC sur les bornes Vin (+) et GND (-) 
 	
Connection au WiFi 
	- Si le module n'arrive pas à se connecter: 
		- La led rouge est allumée.
		- Il se met en mode point d'accès WiFi.
		- Avec un ordinateur, une tablette ou un téléphone, on va dans les paramètres du Wifi et on choisis le point d'accès PUGHxxxxxx.
		- On se rend dans le navigateur web
		- Une page web s'ouvre et on peut y entrer les paramètrs du WiFi.
		- Le module redémarre en se connectant au WiFi.
	- Le module va garder en mémoire les paramètres du WiFi et se reconnectera seul la prochaine fois.  	
		
Détection de la sonde thermométrique
	- Au démarrage, le module recherche l'adresse du thermomètre automatiquement.
	- Il n'y a rien à configurer.

Mesures
	- La température est mesurée toutes les 10 secondes et la moyenne du dernier quart d'heure est calculée.
	- Le débit est mesuré toutes les 10 secondes et le total est gardé en mémoire.
	- Les données sont transmises chaque 15 minutes (0, 15, 30 et 45).
	- Le format est CSV (Date et Heure, Numéro du module, Température et Débit).
	- Les données peuvent êtres suivies en temps réel via l'interface web.

Configuration
	- Le module est configurable via l'interface web
		- Les paramètres de connexion FTP
		- Les paramètres du débimètre (Débit maximum, Pulses par litre, Calibrage).	

Alarme 
	- Aucune alarme n'est actuellement prévue.
	
Serveur Web
	- Le server web sert à se connecter en local avec une tablette ou un téléphone. 
		Pour les téléphones IOS, le browser permet de se connecter directement en entrant PUGHxxxxxx.local.
		Pour les téléphones Android, il faut installer une petite application Bonjour Browser. 
		Cette application affiche tous les services disponibles sur le WiFi local. Choisir PUGHxxxxxx.local.  
	
Code 
	Common.h
		Contient les variables globales accessibles par les autres fichiers
	
	Debug.h
		Contient une liste de constantes qui permettent d'afficher des traces dans le code. 
		Commenter les traces qui ne sont pas nécessaires. 
	
	FTP.cpp, FTP.h
		Code du client FTP
		Une seule fonction est disponible pour envoyer un fichiers : doFtp(...)
		
	RTC.cpp, RTC.h
		Code pour synchroniser l'heure du système avec un serveur NTP
		Changer les 2 lignes suivantes pour ajuster l'heure d'été et d'hiver à la Belgique
		TimeChangeRule myDST = {"EDT", Second, Sun, Mar, 1, +0}; // commence le second dimanche de mars, fuseau, hiver  
		TimeChangeRule mySTD = {"STD", Second, Sun, Mar, 1, +60};// commence le second dimanche de mars, fuseau, été
		
	Svr.cpp, Svr.h
		Code du server web
		Pour ajouter une page ou une commande, il faut: 
			l'ajouter à la fonction startServer(). Ex: server.on("/newPage", handleNewPage);
			Ajouter la nouvelle fonction handleNewPage().
			Ajouter le fichier xxx.htm dans le répertoire Data.
			Uploader les fichiers (voir Compilation).
			
	Main.cpp
		Code principal qui se sert de tous les autres
		Les deux fonction principales 
			setup() : cette fonction est appelée une seule fois au démarrage ou au reset du module.
				- Se connecte au WiFi et affiche l'adresse IP qui lui est attribuée par le WiFi.
				- Démarre le mDNS pour avoir accès au module via l'url PUGHxxxxxx.local.
				- Démarre le disque virtuel (SPIFFS) et y crée un fichiers.
				- Lit le fichier de configuration dans le disque virtuel.
				- Initialise les interruptions pour lire les impulsions du débimètre.
				- Synchronise la date et l'heure.
				- Lit l'adresse du DS18B20 pour pouvoir lire la température.
				- Génère le nom du fichier à utiliser pour le serveur FTP.
				- Affiche le contenu du disque virtuel.
				
			loop() : cette fonction est appelée apr`s le setup et boucle continuellement.
				- Update le serveur mDNS.
				- Execute les commandes reçues par le serveur Web.	
				- Execute les différentes fonctions à des temps bien précis.
					- Toutes les 10 secondes:
						- lit la température et lance une nouvelle mesure.
						- lit de débit.
					- Toutes les minutes : rien actuellement
					- Toutes les 15 minutes : 
						- Ajoute une ligne au fichier local. 
						- Envoie ce fichier par FTP. 
					- Une fois par jour a minuit
						- Supprime la fichier local (les données viennent d'être envoyées).
						- Crée un nouveau nom de fichier pour les nouvelles mesures.  
						
Compilation
	- Copier le contenu du fichier zip dans un répertoire sous mes documents, Projects.
	- Uploader les fichiers sur le disque virtuel (SPIFFS):
		- Fermer la fenêtre du terminal si elle est ouverte.
		- Cliquer sur l'icone avec une page en bas de l'écran.
		- Choisir Upload data.
		- Vérifier qu'il n'y a pas d'erreur.
	- Compiler le code:
		- Cliquer sur l'icone avec une flèche ->
		- Vérifier qu'il n'y a pas d'erreur.
		- Cliquer sur l'icone avec une prise de courant.
		- Suivre les messages sur le moniteur. 	
		
		
		
		

		
	
	




	
	

	
	