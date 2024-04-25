import sys
import json
import base64
import requests
import qdarktheme
import webbrowser
from PyQt5.QtCore import Qt, pyqtSignal, QSize
from PyQt5.QtWidgets import QApplication, QWidget, QVBoxLayout, QGroupBox, QLabel, QTreeWidget, QTreeWidgetItem, QHeaderView, QPushButton, QComboBox, QCheckBox, QScrollArea, QLineEdit, QMessageBox
from PyQt5.QtGui import QIcon

import os
import time

import random

import socket
MOOLAHNET_SOCKET_PORT = 20250
MOOLAHNET_MWS_ID = "47524"
MOOLAHNET_MWS_ENABLED = True

config_file_path = "./MoolahNet-Config.json"
class Config():
    def __init__(self):
        self.load()

    def save(self):
        fp = open(config_file_path, 'w')
        json.dump(self.config_data, fp)
        fp.close()
        pass
    
    def load(self):
        if os.path.exists(config_file_path):
            fp = open(config_file_path, 'r')
            self.config_data = json.load(fp)
            fp.close()
        else:
            self.config_data = {}
            self.config_data["token"] = ""
            self.config_data["token_expires"] = 0
            self.config_data["refresh_token"] = ""
            self.config_data["refresh_token_expires"] = 0
            self.save()
        pass
    
    def token(self):
        return self.config_data["token"]
    def refresh(self):
        return self.config_data["refresh_token"]
    def refresh_expires(self):
        return self.config_data["refresh_token_expires"]
    
    def token_or_refresh_available(self):
        return time.time() < self.config_data["token_expires"] or time.time() < self.refresh_expires()

    def token_expired(self):
        return time.time() > self.config_data["token_expires"]
    
    def set_token(self, token, expires):
        self.config_data["token"] = token
        self.config_data["token_expires"] = expires
    def set_refresh(self, refresh, expires):
        self.config_data["refresh_token"] = refresh
        self.config_data["refresh_token_expires"] = expires
        

config = Config()

def refreshToken():
    url = 'https://nebula.starbreeze.com/iam/v3/oauth/token'
    client_id = '0b3bfdf5a25f452fbd33a36133a2deab' + ':'
    headers = {
        'Authorization': f'Basic {base64.b64encode(client_id.encode()).decode()}',
        'Content-Type': 'application/x-www-form-urlencoded'
    }
    data = {
        'grant_type': 'refresh_token',
        'refresh_token': config.refresh(),
        'extend_exp': True
    }

    response = requests.post(url, headers=headers, data=data)
    if response.status_code == 200:
        response_json = response.json()
        config.set_token(response_json["access_token"], time.time() + response_json["expires_in"])
        config.set_refresh(response_json["refresh_token"], time.time() + response_json["refresh_expires_in"])
        config.save()
    

already_sent_modworkshop_request = False #only send the modworkshop api request once, we dont want to spam it's servers
cached_modworkshop_version = ""
def getLatestMWSVersion():
    global already_sent_modworkshop_request
    global cached_modworkshop_version
    if not already_sent_modworkshop_request:
        req = requests.request("GET", f"https://api.modworkshop.net/mods/{MOOLAHNET_MWS_ID}/version")
        print(f"Requesting ModWorkshop version, got status code {req.status_code}")
        if req.status_code == 200:
            #print(req.content)
            cached_modworkshop_version = req.content.decode()
            already_sent_modworkshop_request = True
    return cached_modworkshop_version
    
owned_dlc = {}
owned_dlc["Syntax Error"] = False
def getOwnedDLC(token):
    syntax_ids = ["9b3e2be8772e4674bb85dc28f533924e", "31a224936c0846799ce55efaa3245d9d", "db780556b7c2474fbe790ebd0bf8cc61", "a6f6219f6ab04b3b9d90ae7c6778b379"]
    for id in syntax_ids:
        url = f"https://nebula.starbreeze.com/platform/public/namespaces/pd3/users/me/entitlements/ownership/any?itemIds={id}"
        header = {
            "Authorization": "Bearer " + token,
            "Accept": "application/json"
        }
        req = requests.request("GET", url=url, headers=header)
        if req.status_code == 200:
            if json.loads(req.content.decode())["owned"]:
                owned_dlc["Syntax Error"] = True
                break

class MoolahNetSocket():
    def __init__(self):
      self.has_connected = False
      self.game_socket = socket.socket()
      self.done_version_check = False
      pass

    def connectToSocket(self, initial_connection_program_start = False):
        global MOOLAHNET_MWS_ENABLED
        self.game_socket = socket.socket()
        try:
          self.game_socket.connect(("127.0.0.1", MOOLAHNET_SOCKET_PORT))
          self.has_connected = True
          if not self.done_version_check and MOOLAHNET_MWS_ENABLED:
              mwsversion = getLatestMWSVersion()
              print(f"ModWorkshop Version: {str(mwsversion)}")
              #cppversion = self.getCPPModVersion()
              cppversion = "0.4.2" # hacky fix since I cant update c++ side at the moment
              print(f"Game version: {str(cppversion)}")
              #if cppversion != mwsversion:
              if cppversion != mwsversion:
                  messagebox = QMessageBox()
                  messagebox.setWindowTitle("MoolahNet")
                  messagebox.setText(f"Version mismatch with ModWorkshop, mod reported version {cppversion}, while ModWorkshop has version {mwsversion}.\n\nVisit ModWorkshop to get the latest version?")
                  yesbutton = QPushButton("Yes")
                  nobutton = QPushButton("No")

                  def visitModworkshop():
                      webbrowser.open(f"https://modworkshop.net/mod/{MOOLAHNET_MWS_ID}")
                  yesbutton.pressed.connect(visitModworkshop)


                  messagebox.addButton(yesbutton, QMessageBox.ButtonRole.YesRole)
                  messagebox.addButton(nobutton, QMessageBox.ButtonRole.NoRole)
                  messagebox.show()
                  messagebox.exec_()
        except Exception:
            
            #typ, value, traceback = sys.exc_info()
            #print(f"{typ}: {value}\n{traceback}")

            if not initial_connection_program_start:
                messagebox_ = QMessageBox()
                messagebox_.setWindowTitle("MoolahNet")
                messagebox_.setText("Unable to connect to game, are you sure you have the mod installed?")
                messagebox_.show()
                messagebox_.exec_()
    
    def send(self, data):
        try:
            self.game_socket.send(data)
        except:
            while True:
                print("Socket disconnected, reconnecting")
                self.connectToSocket()
                if self.game_socket.fileno() != -1:
                    self.game_socket.send(data)
                    break
                time.sleep(0.5)
    
    def recv(self, buffsize):
        return self.game_socket.recv(buffsize)
    
    def getCPPModVersion(self):
        self.send("version".encode())
        return self.recv(10).decode().replace("\x00", "") # remove trailing NULL byte in version string

    def connected(self):
        return self.has_connected and self.game_socket.fileno() != -1

class LoginWindow(QWidget):
    loginSuccessful = pyqtSignal(str)

    def __init__(self):
        super().__init__()
        self.initUI()

    def initUI(self):
        self.setFixedSize(400, 300)

        self.setWindowTitle("MoolahNet Login")
        layout = QVBoxLayout()

        self.username_input = QLineEdit()
        self.username_input.setPlaceholderText("Nebula Email")
        layout.addWidget(self.username_input)

        self.password_input = QLineEdit()
        self.password_input.setPlaceholderText("Nebula Password")
        self.password_input.setEchoMode(QLineEdit.Password)
        layout.addWidget(self.password_input)

        self.disclaimer_checkbox = QCheckBox(" I understand that this open-source software is not affiliated \n with New Starbreeze AB or its subsidiaries.")
        self.disclaimer_checkbox.stateChanged.connect(self.updateLoginButtonState)
        layout.addWidget(self.disclaimer_checkbox)

        self.save_token = QCheckBox(" Save my login token locally to avoid logging in again.")
        self.save_token.stateChanged.connect(self.updateSaveTokenButtonState)
        layout.addWidget(self.save_token)

        self.login_button = QPushButton("Login")
        self.login_button.clicked.connect(self.login)
        self.login_button.setEnabled(False)  # Initially disable login button
        layout.addWidget(self.login_button)
        
        self.setLayout(layout)
        self.setBaseSize(layout.sizeHint())

    def updateLoginButtonState(self):
        if self.disclaimer_checkbox.isChecked():
            self.login_button.setEnabled(True)
        else:
            self.login_button.setEnabled(False)
    
    def updateSaveTokenButtonState(self):
        self.save_token_to_config = self.save_token.isChecked()

    def login(self):
        username = self.username_input.text()
        password = self.password_input.text()

        url = 'https://nebula.starbreeze.com/iam/v3/oauth/token'
        client_id = '0b3bfdf5a25f452fbd33a36133a2deab' + ':'
        headers = {
            'Authorization': f'Basic {base64.b64encode(client_id.encode()).decode()}',
            'Content-Type': 'application/x-www-form-urlencoded'
        }
        data = {
            'grant_type': 'password',
            'username': username,
            'password': password,
            'extend_exp': True
        }

        response = requests.post(url, headers=headers, data=data)

        if response.status_code == 200:
            response_json = response.json()
            self.token = response_json["access_token"]
            self.token_expires = time.time() + response_json["expires_in"]
            self.refresh_token = response_json["refresh_token"]
            self.refresh_expires = time.time() + response_json["refresh_expires_in"]

            self.hide()
            self.loginSuccessful.emit(self.token)
        else:
            QMessageBox.critical(self, "Login Error", "Make sure that your username and password are valid!", QMessageBox.Ok)

def checkVersion(session):
    try:
        return session["attributes"]["party_code"][0] == ""
    except:
        try:
            return session["attributes"]["PARTY_CODE"][0] == ""
        except:
            msg = QMessageBox()
            msg.setWindowTitle("MoolahNet")
            msg.setText("An unexpected error has occured, please send error_dump.json to the mod author and continue to use the mod as normal.")
            open("error_dump.json", "w").write(json.dumps(session))
            msg.show()
            msg.exec_()
            return False

class ServerBrowser(QWidget):
    def __init__(self, moolahsocket):
        super().__init__()
        self.sessions = None
        self.token = None
        self.game_socket = moolahsocket
        self.initUI()

    def initUI(self):
        self.setFixedSize(800, 600)

        layout = QVBoxLayout()
        self.groupbox = QGroupBox("Server Browser")
        vbox = QVBoxLayout()

        self.server_tree = QTreeWidget()
        self.server_tree.setHeaderLabels(["Map", "Difficulty", "Players", "Region", "Cross-Platform", "ID"])
        self.server_tree.setSortingEnabled(True)
        vbox.addWidget(self.server_tree)

        # Add filter dropdowns for map names, difficulty, platform
        self.map_filter_dropdown = QComboBox()
        self.map_filter_dropdown.addItem("All Maps")
        self.map_filter_dropdown.currentIndexChanged.connect(self.populateServerTree)
        vbox.addWidget(self.map_filter_dropdown)

        self.difficulty_filter_dropdown = QComboBox()
        self.difficulty_filter_dropdown.addItem("All Difficulties")
        self.difficulty_filter_dropdown.addItems(["Normal", "Hard", "Very Hard", "Overkill"])
        self.difficulty_filter_dropdown.currentIndexChanged.connect(self.populateServerTree)
        vbox.addWidget(self.difficulty_filter_dropdown)

        self.platform_filter_dropdown = QComboBox()
        self.platform_filter_dropdown.addItem("All Platforms")
        self.platform_filter_dropdown.addItems(["Steam", "EpicGames", "PS5", "XBOX"])
        self.platform_filter_dropdown.currentIndexChanged.connect(self.populateServerTree)
        vbox.addWidget(self.platform_filter_dropdown)

        # Add checkboxes for regions
        self.region_checkboxes = []
        self.region_layout = QVBoxLayout()

        # Create a scroll area for the region filter group box
        scroll_area = QScrollArea()
        scroll_area.setWidgetResizable(True)
        scroll_area.setHorizontalScrollBarPolicy(Qt.ScrollBarAlwaysOff)

        # Create a widget to contain the region layout
        region_widget = QWidget()
        region_widget.setLayout(self.region_layout)

        # Set the widget containing the region layout as the scroll area's widget
        scroll_area.setWidget(region_widget)

        # Set fixed maximum height for the scroll area box
        scroll_area.setMaximumHeight(150)

        # Add the scroll area to the main layout
        vbox.addWidget(scroll_area)

        # Add tickbox for joinable lobbies
        self.joinable_lobbies_checkbox = QCheckBox("Only find joinable lobbies (hide full lobbies)")
        self.joinable_lobbies_checkbox.setChecked(True)
        self.joinable_lobbies_checkbox.stateChanged.connect(self.populateServerTree)
        vbox.addWidget(self.joinable_lobbies_checkbox)

        # Add a refresh button
        self.refresh_button = QPushButton("Refresh")
        self.refresh_button.clicked.connect(self.refreshServerBrowser)
        vbox.addWidget(self.refresh_button)

        # Add button to join server
        self.join_button = QPushButton("Join")
        self.join_button.clicked.connect(self.joinServer)
        self.join_button.setEnabled(False)  # Initially disabled
        vbox.addWidget(self.join_button)

        self.random_session_button = QPushButton("Quickplay - Join random session based on filter criteria")
        self.random_session_button.clicked.connect(self.quickplay)
        self.random_session_button.setEnabled(True)
        vbox.addWidget(self.random_session_button)

        # Hide the ID column (last column)
        self.id_column_index = self.server_tree.columnCount() - 1
        self.server_tree.setColumnHidden(self.id_column_index, True)

        self.server_tree.selectionModel().selectionChanged.connect(self.updateJoinButtonState)  # Connect selectionChanged signal
        self.server_tree.itemDoubleClicked.connect(self.openPlayerProfile)  # Connect double click event
        #self.populateServerTree()

        self.groupbox.setLayout(vbox)
        layout.addWidget(self.groupbox)

        self.setLayout(layout)
        self.setMinimumHeight(layout.sizeHint().height() + 300)
        self.setMinimumWidth(layout.sizeHint().width() + 600)
        self.setWindowTitle("MoolahNet")
        

    # Define a map for the level name resolution
    level_map = {
        "BranchBank": "No Rest For the Wicked",
        "ArmoredTransport": "Road Rage",
        "JewelryStore": "Dirty Ice",
        "NightClub": "Rock the Cradle",
        "ArtGallery": "Under the Surphaze",
        "FirstPlayable": "Gold and Sharke",
        "CargoDock": "99 Boxes",
        "Penthouse": "Touch the Sky",
        "Villa": "Cook Off",
        "Station": "Turbid Station",
        "DataCenter": "Syntax Error"
    }

    def map_level_to_name(self, internal_name):
        return self.level_map.get(internal_name, internal_name)

    # Define a map for the difficulty index resolution
    difficulty_map = {
        0: "Normal",
        1: "Hard",
        2: "Very Hard",
        3: "Overkill"
    }

    def map_index_to_difficulty(self, idx):
        return self.difficulty_map.get(int(idx), "Unknown")

    # Define a map for the region name resolution
    region_map = {
        "us-east-2": "US East (Ohio)",
        "us-east-1": "US East (Virginia)",
        "us-west-1": "US West (N. California)",
        "us-west-2": "US West (Oregon)",
        "af-south-1": "Africa (Cape Town)",
        "ap-east-1": "Asia Pacific (Hong Kong)",
        "ap-south-2": "Asia Pacific (Hyderabad)",
        "ap-southeast-3": "Asia Pacific (Jakarta)",
        "ap-southeast-4": "Asia Pacific (Melbourne)",
        "ap-south-1": "Asia Pacific (Mumbai)",
        "ap-northeast-3": "Asia Pacific (Osaka)",
        "ap-northeast-2": "Asia Pacific (Seoul)",
        "ap-southeast-1": "Asia Pacific (Singapore)",
        "ap-southeast-2": "Asia Pacific (Sydney)",
        "ap-northeast-1": "Asia Pacific (Tokyo)",
        "ca-central-1": "Canada (Central)",
        "ca-west-1": "Canada West (Calgary)",
        "eu-central-1": "Europe (Frankfurt)",
        "eu-west-1": "Europe (Ireland)",
        "eu-west-2": "Europe (London)",
        "eu-south-1": "Europe (Milan)",
        "eu-west-3": "Europe (Paris)",
        "eu-south-2": "Europe (Spain)",
        "eu-north-1": "Europe (Stockholm)",
        "eu-central-2": "Europe (Zurich)",
        "il-central-1": "Israel (Tel Aviv)",
        "me-south-1": "Middle East (Bahrain)",
        "me-central-1": "Middle East (UAE)",
        "sa-east-1": "South America (São Paulo)"
    }

    def region_code_to_name(self, region_code):
        return self.region_map.get(region_code, region_code)

    def getUniqueMaps(self):
        maps = set()
        for session in self.sessions["data"]:
            map_asset_names = self.extractMapNames(session["attributes"])
            if isinstance(map_asset_names, str):
                maps.add(map_asset_names)
            elif isinstance(map_asset_names, list):
                maps.update(map_asset_names)
        return sorted(list(maps))

    def extractMapNames(self, attributes):
        map_asset_names = attributes.get("MAPNAME")
        if map_asset_names is None:
            map_asset_names = attributes.get("MapAssetTestName", attributes.get("MapAssetNameTest", attributes.get("MAPASSETNAMETEST")))

        for idx in range(len(map_asset_names)):
            map_asset_names[idx] = self.map_level_to_name(map_asset_names[idx])

        return map_asset_names

    def getUniqueRegions(self):
        regions = set()
        for session in self.sessions["data"]:
            region = session.get("DSInformation", {}).get("Server", {}).get("region")
            if region:
                regions.add(region)

        unique_regions = list(regions)

        for idx in range(len(unique_regions)):
            unique_regions[idx] = self.region_code_to_name(unique_regions[idx])

        return sorted(unique_regions)

    def showServerBrwoser(self, token):
        self.token = token
        self.refreshServerBrowser()
        self.show()

    def refreshServerBrowser(self):
        self.getSessionData()

        # Update the filter contents
        unique_maps = self.getUniqueMaps()
        existing_maps = [self.map_filter_dropdown.itemText(i) for i in range(self.map_filter_dropdown.count())]
        new_maps = set(unique_maps) - set(existing_maps)
        new_maps = list(new_maps)
        new_maps.sort()
        for map in new_maps:
            if map == "Syntax Error":
                if owned_dlc["Syntax Error"]:
                    self.map_filter_dropdown.addItem(map)
                else:
                    continue
            else:
                self.map_filter_dropdown.addItem(map)

        existing_regions = [checkbox.text() for checkbox in self.region_checkboxes]
        unique_regions = self.getUniqueRegions()
        new_regions = set(unique_regions) - set(existing_regions)

        for region in new_regions:
            checkbox = QCheckBox(region)
            checkbox.setChecked(True)  # Initially all regions are selected
            checkbox.stateChanged.connect(self.populateServerTree)
            self.region_layout.addWidget(checkbox)
            self.region_checkboxes.append(checkbox)

        self.populateServerTree()

    def getSessionData(self):
        url = 'https://nebula.starbreeze.com/session/v1/public/namespaces/pd3/gamesessions?availability=all'
        headers = {
            'Authorization': f'Bearer ' + self.token,
            'Content-Type': 'application/json'
        }
        data = {
            'limit': 10000
        }

        response = requests.post(url, headers=headers, json=data)

        if response.status_code == 200:
            self.sessions = response.json()
        else:
            if config.token_expired():
                refreshToken()
                self.token = config.token()
                headers["Authorization"] = f'Bearer ' + self.token
                response = requests.post(url, headers=headers, json=data)
                if response.status_code == 200:
                    self.sessions = response.json()
                else:
                    QMessageBox.critical(self, "API Error", "Failed to fetch Nebula Sessions!", QMessageBox.Ok)
            else:
                QMessageBox.critical(self, "API Error", "Failed to fetch Nebula Sessions!", QMessageBox.Ok)

    def populateServerTree(self):
        self.server_tree.clear()
        selected_map = self.map_filter_dropdown.currentText()
        selected_difficulty = self.difficulty_filter_dropdown.currentText()
        selected_platform = self.platform_filter_dropdown.currentText().upper()
        selected_regions = [checkbox.text() for checkbox in self.region_checkboxes if checkbox.isChecked()]
        only_joinable = self.joinable_lobbies_checkbox.isChecked()

        # Statistic stuff, not important
        active_sessions = 0
        total_players = 0

        player_breakdown = {}
        player_breakdown["EPICGAMES"] = 0
        player_breakdown["STEAM"] = 0
        player_breakdown["XBOX"] = 0
        player_breakdown["PS5"] = 0

        player_breakdown["ERRORPLATFORM"] = 0

        # For each session that the API returns
        for session in self.sessions["data"]:
            session_attributes = session["attributes"]
            map_asset_names = self.extractMapNames(session_attributes)

            if (selected_map == "All Maps" or selected_map in map_asset_names) and \
               (selected_difficulty == "All Difficulties" or self.map_index_to_difficulty(session_attributes.get("DifficultyIdx", session_attributes.get("DIFFICULTYIDX", 99))) == selected_difficulty):
                
                # Get a set of all players linked to their matching platforms
                player_platforms = set()
                player_count = 0
                
                
                if not checkVersion(session):
                    continue

                for member in session["members"]:
                    if member.get("status") == "JOINED":
                        player_count += 1
                        platform = member.get("platformID", "UNKNOWN").upper()
                        player_platforms.add((f"Player {player_count} - {platform}", member.get("platformUserID", "UNKNOWN").upper()))

                        if platform == "":
                            platform = "ERRORPLATFORM"
                        player_breakdown[platform] += 1


                # Find out if a lobby is actually cross platform (contains players from more than one platform) as a visual indicator
                platforms = list()
                for entry in player_platforms:
                    platforms.append(entry[0].split(" - ")[1])
                is_cross_platform = len(set(platforms)) > 1 and player_count >= 2

                # Hide all lobbies that do not have crossplay enabled
                crossplay_platforms = session_attributes.get("cross_platform", session_attributes.get("CROSS_PLATFORM", "UNKNOWN"))
                if "steam" not in crossplay_platforms and "epicgames" not in crossplay_platforms and "eos" not in crossplay_platforms:
                    continue
                
                # Hide all game sessions that don't have a dedicated server
                if session.get("DSInformation", {}).get("StatusV2", "") != "AVAILABLE":
                    continue


                if session_attributes.get("MapAssetTestName", session_attributes.get("MapAssetNameTest", session_attributes.get("MAPASSETNAMETEST")))[0] == "Syntax Error":
                    if not owned_dlc["Syntax Error"]:
                        continue

                # Check all the filters and fill out the server tree data
                session_region = self.region_code_to_name(session.get("DSInformation", {}).get("Server", {}).get("region"))
                if not selected_regions or session_region in selected_regions:
                    if selected_platform == "ALL PLATFORMS" or selected_platform in platforms:
                        server_item = QTreeWidgetItem(self.server_tree)
                        server_item.setText(0, map_asset_names[0])

                        server_item.setText(1, self.map_index_to_difficulty(session_attributes.get("DifficultyIdx", session_attributes.get("DIFFICULTYIDX", 99))))
                        server_item.setText(2, f"{player_count}/4 players")
                        server_item.setText(3, session_region)
                        server_item.setText(4, str(is_cross_platform))
                        if player_count > 0:
                            active_sessions += 1
                            total_players += player_count
                        #server_item.setText(self.id_column_index, session.get('id', 'Unknown ID'))
                        server_item.setText(self.id_column_index, json.dumps(session))

                        for player_info in player_platforms:
                            member_item = QTreeWidgetItem(server_item)
                            member_item.setText(0, player_info[0])
                            member_item.setText(self.id_column_index, player_info[1])
 
                        # If servers are empty (will be closed) or full (API should not report these but you never know)
                        if only_joinable and (player_count == 0 or player_count == 4):
                            self.server_tree.invisibleRootItem().removeChild(server_item)
        
        #self.server_count.setText(f"Found {self.server_tree.invisibleRootItem().childCount()} sessions")
        self.groupbox.setTitle(f"Server Browser - {self.server_tree.invisibleRootItem().childCount()} sessions")
        # Statistics, can be removed
        #print(f"\n\n\nFound {active_sessions} active sessions\n\n\nWith {total_players} total players\n\n\n")

        #print("Platform breakdown:")
        #for platform in player_breakdown:
        #    print(f"{platform} - {player_breakdown[platform]} players")

    def joinServer(self):
        selected_items = self.server_tree.selectedItems()
        if selected_items:
            server_id = selected_items[0].text(self.id_column_index)
            #open("server.json", "w").write(server_id)
            
            # Call the join function with the server ID
            if not self.game_socket.connected():
                self.connectToGameServer()

            if self.game_socket.connected():
                print(f"Joining server {server_id}")
                try:
                    self.game_socket.send(server_id.encode())
                except: # Socket has disconnected
                    self.connectToGameServer()
                    self.game_socket.send(server_id.encode())

    def quickplay(self):
        session_idx = random.randint(0, self.server_tree.invisibleRootItem().childCount()-1)

        session = self.server_tree.invisibleRootItem().child(session_idx).text(self.id_column_index)
        if not self.game_socket.connected():
            self.connectToGameServer()

        if self.game_socket.connected():
            print(f"Joining server {session}")
            try:
                self.game_socket.send(session.encode())
            except: # Socket has disconnected
                self.connectToGameServer()
                self.game_socket.send(session.encode())
        pass

    def connectToGameServer(self):
        self.game_socket.connectToSocket()
        print("Connecting to game socket")

    def openPlayerProfile(self, item, column):
        player_info = item.text(column)
        if "STEAM" in player_info:
            steam_id = item.text(self.id_column_index)
            steam_profile_url = f"https://steamcommunity.com/profiles/{steam_id}"
            webbrowser.open(steam_profile_url)

    def resizeEvent(self, event):
        super().resizeEvent(event)
        self.server_tree.header().setSectionResizeMode(0, QHeaderView.ResizeToContents)

    def updateJoinButtonState(self):
        selected_items = self.server_tree.selectedItems()
        if selected_items:
            self.join_button.setEnabled(True)
        else:
            self.join_button.setEnabled(False)


if __name__ == '__main__':
    app = QApplication(sys.argv)
    qdarktheme.setup_theme()
    app.setWindowIcon(QIcon("./icon.png"))
    
    config.load()

    moolahsocket = MoolahNetSocket()

    server_browser = ServerBrowser(moolahsocket)
    
    #remove later
    moolahsocket.connectToSocket(initial_connection_program_start=True)

    if config.token_or_refresh_available():
        if not config.token_expired():
            # use token
            getOwnedDLC(config.token())
            server_browser.showServerBrwoser(config.token())
            sys.exit(app.exec_())
        else:
            # use refresh to get a new token
            refreshToken()
            getOwnedDLC(config.token())
            server_browser.showServerBrwoser(config.token())
            sys.exit(app.exec_())
    else:
        # login window
        login_window = LoginWindow()
        login_window.show()
        def logged_in(token):
            config.set_token(login_window.token, login_window.token_expires)
            config.set_refresh(login_window.refresh_token, login_window.refresh_expires)
            if login_window.save_token_to_config:
                config.save()
            getOwnedDLC(login_window.token)
            server_browser.showServerBrwoser(login_window.token)
        login_window.loginSuccessful.connect(logged_in)
        sys.exit(app.exec_())
