/*
	Copyright(c) 2021-2023 jvde.github@gmail.com

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "DB.h"

//-----------------------------------
// simple ship database

const std::string null_str = "null";
const std::string comma = ",";

void DB::setup(float lt, float ln) {

	if (server_mode) {
		N *= 32;
		M *= 32;

		std::cerr << "DB: internal ship database extended to " << N << " ships and " << M << " path points" << std::endl;
	}

	ships.resize(N);
	std::memset(ships.data(), 0, N * sizeof(ShipList));
	paths.resize(M);
	std::memset(paths.data(), 0, M * sizeof(PathList));

	lat = lt;
	lon = ln;

	first = N - 1;
	last = 0;
	count = 0;
	// set up linked list
	for (int i = 0; i < N; i++) {
		ships[i].next = i - 1;
		ships[i].prev = i + 1;
	}
	ships[N - 1].prev = -1;
}

std::vector<std::pair<int, int>> validMID = {
    {201, 220}, {224, 279}, {301, 312}, {314, 316}, {319, 321}, {323, 325},
    {327, 329}, {330, 332}, {334, 336}, {338, 339}, {341, 343}, {345, 347},
    {348, 350}, {351, 359}, {361, 362}, {364, 366}, {367, 379}, {401, 403},
    {405, 405}, {408, 410}, {412, 414}, {416, 417}, {419, 419}, {422, 423},
    {425, 425}, {428, 431}, {432, 432}, {434, 436}, {437, 438}, {440, 441},
    {443, 443}, {445, 447}, {450, 451}, {453, 453}, {455, 457}, {459, 459},
    {461, 463}, {466, 468}, {470, 473}, {475, 477}, {478, 478}, {501, 503},
    {506, 508}, {510, 512}, {514, 515}, {516, 516}, {518, 520}, {523, 525},
    {529, 529}, {531, 533}, {536, 538}, {540, 542}, {544, 546}, {548, 550},
    {553, 555}, {557, 559}, {561, 563}, {564, 567}, {570, 572}, {574, 576},
    {577, 578}, {601, 603}, {605, 607}, {608, 613}, {615, 622}, {624, 627},
    {629, 630}, {631, 638}, {642, 644}, {645, 647}, {649, 650}, {654, 657},
    {659, 669}, {670, 672}, {674, 679}, {701, 701}, {710, 710}, {720, 720},
    {725, 725}, {730, 730}, {735, 735}, {740, 740}, {745, 745}, {750, 750},
    {755, 755}, {760, 760}, {765, 765}, {770, 770}, {775, 775}
};

bool isValidMID(int value) {
    return std::any_of(validMID.begin(), validMID.end(), [value](const auto& range) {
        return value >= range.first && value <= range.second;
    });
}

int getIconID(int mmsi) {
    if (mmsi >= 201000001 && mmsi <= 775999999 && isValidMID(mmsi / 1000000))
        return 0; // Ship MIDxxxxxx
    if (mmsi >= 2010001 && mmsi <= 7759999 && isValidMID(mmsi / 10000))
        return 1; // Basestation 00MIDxxxx
    if (mmsi >= 992010001 && mmsi <= 997759999 && isValidMID((mmsi - 990000000) / 10000))
        return 2; // Aton 99MIDxxxx
    if (mmsi >= 111201001 && mmsi <= 111775999 && isValidMID((mmsi - 111000000) / 1000))
        return 3; // SAR 111MIDxxx
    if (mmsi >= 20100001 && mmsi <= 77599999 && isValidMID(mmsi / 100000))
        return 4; // ShipGroup 0MIDxxxxx
    if (mmsi >= 97000001 && mmsi <= 97099999)
        return 5; // AIS SART is identified by the number 970 and 6 digits,	
    return 6; // USO Unindentified Sailing Object
}

bool DB::isValidCoord(float lat, float lon) {
	return !(lat == 0 && lon == 0) && lat != 91 && lon != 181;
}

// https://www.movable-type.co.uk/scripts/latlong.html
void DB::getDistanceAndBearing(float lat1, float lon1, float lat2, float lon2, float& distance, int& bearing) {
	// Convert the latitudes and longitudes from degrees to radians
	lat1 = deg2rad(lat1);
	lon1 = deg2rad(lon1);
	lat2 = deg2rad(lat2);
	lon2 = deg2rad(lon2);

	// Compute the distance using the haversine formula
	float dlat = lat2 - lat1, dlon = lon2 - lon1;
	float a = sin(dlat / 2) * sin(dlat / 2) + cos(lat1) * cos(lat2) * sin(dlon / 2) * sin(dlon / 2);
	distance = 2 * EarthRadius * NauticalMilePerKm * asin(sqrt(a));

	float y = sin(dlon) * cos(lat2);
	float x = cos(lat1) * sin(lat2) - sin(lat1) * cos(lat2) * cos(dlon);
	bearing = rad2deg(atan2(y, x));
}

// add member to get JSON in form of array with values and keys seperately
std::string DB::getJSONcompact(bool full) {
    std::stringstream ss;

    ss << "{\"count\":" << std::to_string(count) << comma;
    if (latlon_share)
        ss << "\"station\":{\"lat\":" << std::to_string(lat) << ",\"lon\":" << std::to_string(lon) << "},";

    ss << "\"values\":[";

    std::time_t tm = time(nullptr);
    int ptr = first;

    std::string delim = "";
    while (ptr != -1) {
        const VesselDetail ship = ships[ptr].ship;
        if (ship.mmsi != 0) {
            long int delta_time = static_cast<long int>(tm) - static_cast<long int>(ship.last_signal);
            if (!full && delta_time > TIME_HISTORY)
                break;

            ss << delim << "[" << std::to_string(ship.mmsi) << comma;
            if (isValidCoord(ship.lat, ship.lon)) {
                ss << std::to_string(ship.lat) << comma;
                ss << std::to_string(ship.lon) << comma;

                if (isValidCoord(lat, lon)) {
                    ss << std::to_string(ship.distance) << comma;
                    ss << std::to_string(ship.angle) << comma;
                }
                else {
                    ss << null_str << comma;
                    ss << null_str << comma;
                }
            }
            else {
                ss << null_str << comma;
                ss << null_str << comma;
                ss << null_str << comma;
                ss << null_str << comma;
            }

            ss << std::to_string(ship.level) << comma;
            ss << std::to_string(ship.count) << comma;
            ss << std::to_string(ship.ppm) << comma;
            ss << (ship.approximate ? "true" : "false") << comma;

            ss << ((ship.heading == HEADING_UNDEFINED) ? null_str : std::to_string(ship.heading)) << comma;
            ss << ((ship.cog == COG_UNDEFINED) ? null_str : std::to_string(ship.cog)) << comma;
            ss << ((ship.speed == SPEED_UNDEFINED) ? null_str : std::to_string(ship.speed)) << comma;

            ss << ((ship.to_bow == DIMENSION_UNDEFINED) ? null_str : std::to_string(ship.to_bow)) << comma;
            ss << ((ship.to_stern == DIMENSION_UNDEFINED) ? null_str : std::to_string(ship.to_stern)) << comma;
            ss << ((ship.to_starboard == DIMENSION_UNDEFINED) ? null_str : std::to_string(ship.to_starboard)) << comma;
            ss << ((ship.to_port == DIMENSION_UNDEFINED) ? null_str : std::to_string(ship.to_port)) << comma;

            ss << std::to_string(ship.shiptype) << comma;
            ss << std::to_string(ship.validated) << comma;
            ss << std::to_string(ship.msg_type) << comma;
            ss << std::to_string(ship.channels) << comma;
            ss << "\"" << std::string(ship.country_code) << "\"" << comma;
            ss << std::to_string(ship.status) << comma;

            ss << std::to_string(ship.draught) << comma;

            ss << ((ship.month == ETA_MONTH_UNDEFINED) ? null_str : std::to_string(ship.month)) << comma;
            ss << ((ship.day == ETA_DAY_UNDEFINED) ? null_str : std::to_string(ship.day)) << comma;
            ss << ((ship.hour == ETA_HOUR_UNDEFINED) ? null_str : std::to_string(ship.hour)) << comma;
            ss << ((ship.minute == ETA_MINUTE_UNDEFINED) ? null_str : std::to_string(ship.minute)) << comma;

            ss << ((ship.IMO == IMO_UNDEFINED) ? null_str : std::to_string(ship.IMO)) << comma;

            std::string callsign_json;
            JSON::StringBuilder::stringify(std::string(ship.callsign), callsign_json);
            ss << callsign_json << comma;

            std::string shipname = std::string(ship.shipname) + (ship.virtual_aid ? " [V]" : "");
            std::string shipname_json;
            JSON::StringBuilder::stringify(shipname, shipname_json);
            ss << shipname_json << comma;

            std::string destination_json;
            JSON::StringBuilder::stringify(std::string(ship.destination), destination_json);
            ss << destination_json << comma << std::to_string(delta_time) << "]";
            delim = comma;
        }
        ptr = ships[ptr].next;
    }

    ss << "],\"error\":false}\n\n";

    return ss.str();
}

void DB::getShipJSON(const VesselDetail& ship, std::string& content, long int delta_time) {
    std::stringstream ss;

    ss << "{\"mmsi\":" << std::to_string(ship.mmsi) << ",";
    if (isValidCoord(ship.lat, ship.lon)) {
        ss << "\"lat\":" << std::to_string(ship.lat) << ",";
        ss << "\"lon\":" << std::to_string(ship.lon) << ",";

        if (isValidCoord(lat, lon)) {
            ss << "\"distance\":" << std::to_string(ship.distance) << ",";
            ss << "\"bearing\":" << std::to_string(ship.angle) << ",";
        }
        else {
            ss << "\"distance\":null,";
            ss << "\"bearing\":null,";
        }
    }
    else {
        ss << "\"lat\":null,";
        ss << "\"lon\":null,";
        ss << "\"distance\":null,";
        ss << "\"bearing\":null,";
    }

    ss << "\"level\":" << std::to_string(ship.level) << ",";
    ss << "\"count\":" << std::to_string(ship.count) << ",";
    ss << "\"ppm\":" << std::to_string(ship.ppm) << ",";
    ss << "\"approx\":" << (ship.approximate ? "true" : "false") << ",";

    ss << "\"heading\":" << ((ship.heading == HEADING_UNDEFINED) ? null_str : std::to_string(ship.heading)) << ",";
    ss << "\"cog\":" << ((ship.cog == COG_UNDEFINED) ? null_str : std::to_string(ship.cog)) << ",";
    ss << "\"speed\":" << ((ship.speed == SPEED_UNDEFINED) ? null_str : std::to_string(ship.speed)) << ",";

    ss << "\"to_bow\":" << ((ship.to_bow == DIMENSION_UNDEFINED) ? null_str : std::to_string(ship.to_bow)) << ",";
    ss << "\"to_stern\":" << ((ship.to_stern == DIMENSION_UNDEFINED) ? null_str : std::to_string(ship.to_stern)) << ",";
    ss << "\"to_starboard\":" << ((ship.to_starboard == DIMENSION_UNDEFINED) ? null_str : std::to_string(ship.to_starboard)) << ",";
    ss << "\"to_port\":" << ((ship.to_port == DIMENSION_UNDEFINED) ? null_str : std::to_string(ship.to_port)) << ",";

    ss << "\"shiptype\":" << std::to_string(ship.shiptype) << ",";
    ss << "\"validated\":" << std::to_string(ship.validated) << ",";
    ss << "\"msg_type\":" << std::to_string(ship.msg_type) << ",";
    ss << "\"channels\":" << std::to_string(ship.channels) << ",";
    ss << "\"country\":\"" << std::string(ship.country_code) << "\",";
    ss << "\"status\":" << std::to_string(ship.status) << ",";

    ss << "\"draught\":" << std::to_string(ship.draught) << ",";

    ss << "\"eta_month\":" << ((ship.month == ETA_MONTH_UNDEFINED) ? null_str : std::to_string(ship.month)) << ",";
    ss << "\"eta_day\":" << ((ship.day == ETA_DAY_UNDEFINED) ? null_str : std::to_string(ship.day)) << ",";
    ss << "\"eta_hour\":" << ((ship.hour == ETA_HOUR_UNDEFINED) ? null_str : std::to_string(ship.hour)) << ",";
    ss << "\"eta_minute\":" << ((ship.minute == ETA_MINUTE_UNDEFINED) ? null_str : std::to_string(ship.minute)) << ",";

    ss << "\"imo\":" << ((ship.IMO == IMO_UNDEFINED) ? null_str : std::to_string(ship.IMO)) << ",";

    ss << "\"callsign\":";
    std::string callsign_json;
    JSON::StringBuilder::stringify(std::string(ship.callsign), callsign_json);
    ss << callsign_json << ",";

    ss << "\"shipname\":";
    std::string shipname = std::string(ship.shipname) + (ship.virtual_aid ? " [V]" : "");
    std::string shipname_json;
    JSON::StringBuilder::stringify(shipname, shipname_json);
    ss << shipname_json << ",";

    ss << "\"destination\":";
    std::string destination_json;
    JSON::StringBuilder::stringify(std::string(ship.destination), destination_json);
    ss << destination_json << ",";

    ss << "\"last_signal\":" << std::to_string(delta_time) << "}";

    content += ss.str();
}


std::string DB::getJSON(bool full) {
	std::string str;

	content = "{\"count\":" + std::to_string(count);
	if (latlon_share)
		content += ",\"station\":{\"lat\":" + std::to_string(lat) + ",\"lon\":" + std::to_string(lon) + "}";
	content += ",\"ships\":[";

	std::time_t tm = time(nullptr);
	int ptr = first;

	delim = "";
	while (ptr != -1) {
		const VesselDetail ship = ships[ptr].ship;
		if (ship.mmsi != 0) {
			long int delta_time = (long int)tm - (long int)ship.last_signal;
			if (!full && delta_time > TIME_HISTORY) break;

			content += delim; 
			getShipJSON(ship, content, delta_time);
			delim = ",";
		}
		ptr = ships[ptr].next;
	}
	content += "],\"error\":false}\n\n";
	return content;
}
std::string DB::getShipJSON(int mmsi) {
	int ptr = findShip(mmsi);

	if (ptr == -1) return "{}";
	
	const VesselDetail ship = ships[ptr].ship;		
	long int delta_time = (long int)time(nullptr) - (long int)ship.last_signal;

	std::string content; 
	getShipJSON(ship, content, delta_time);
	return content;
}


std::string DB::getPathJSON(uint32_t mmsi) {
    std::string str;

    int idx = -1;
    for (int i = 0; i < N && idx == -1; i++)
        if (ships[i].ship.mmsi == mmsi) idx = i;

    if (idx == -1) return "[]";

    std::stringstream ss;
    ss << "[";

    int ptr = ships[idx].ship.path_ptr;
    long int t0 = time(nullptr);

    long int t = t0;

    while (ptr != -1 && paths[ptr].mmsi == mmsi && (long int)paths[ptr].signal_time <= t) {
        t = (long int)paths[ptr].signal_time;

        if (isValidCoord(paths[ptr].lat, paths[ptr].lon)) {
            ss << "{\"lat\":" << paths[ptr].lat << ",";
            ss << "\"lon\":" << paths[ptr].lon << ",";
            ss << "\"received\":" << t0 - t << "},";
        }
        ptr = paths[ptr].next;
    }

    std::string content = ss.str();
    if (content != "[") content.pop_back();
    content += "]";
    return content;
}


std::string DB::getMessage(uint32_t mmsi) {
	int ptr = findShip(mmsi);
	if (ptr == -1 || !ships[ptr].ship.msg) return "";
	return *ships[ptr].ship.msg;
}

int DB::findShip(uint32_t mmsi) {
	int ptr = first, cnt = count;
	while (ptr != -1 && --cnt >= 0) {
		if (ships[ptr].ship.mmsi == mmsi) return ptr;
		ptr = ships[ptr].next;
	}
	return -1;
}

int DB::createShip() {
	int ptr = last;
	count = MIN(count + 1, N);
	if(ships[ptr].ship.msg)
		delete ships[ptr].ship.msg;

	ships[ptr].ship = VesselDetail();

	return ptr;
}

void DB::moveShipToFront(int ptr) {
	if (ptr == first) return;

	// remove ptr out of the linked list
	if (ships[ptr].next != -1)
		ships[ships[ptr].next].prev = ships[ptr].prev;
	else
		last = ships[ptr].prev;
	ships[ships[ptr].prev].next = ships[ptr].next;

	// new ship is first in list
	ships[ptr].next = first;
	ships[ptr].prev = -1;

	ships[first].prev = ptr;
	first = ptr;
}

void DB::addToPath(int ptr) {
	paths[path_idx].next = ships[ptr].ship.path_ptr;
	paths[path_idx].lat = ships[ptr].ship.lat;
	paths[path_idx].lon = ships[ptr].ship.lon;
	paths[path_idx].mmsi = ships[ptr].ship.mmsi;
	paths[path_idx].signal_time = ships[ptr].ship.last_signal;

	ships[ptr].ship.path_ptr = path_idx;
	path_idx = (path_idx + 1) % M;
}

bool DB::updateFields(const JSON::Property& p, const AIS::Message* msg, VesselDetail& v, bool allowApproximate) {
	bool position_updated = false;
	if ((msg->type() == 8 || msg->type() == 17 || (msg->type() == 27 && !allowApproximate && !v.approximate)))
		return position_updated;
	switch (p.Key()) {
		// Position Group
		case AIS::KEY_LAT: v.lat = p.Get().getFloat(); position_updated = true; break;
		case AIS::KEY_LON: v.lon = p.Get().getFloat(); position_updated = true; break;
		// Ship Information Group
		case AIS::KEY_SHIPTYPE: v.shiptype = p.Get().getInt(); break;
		case AIS::KEY_IMO: v.IMO = p.Get().getInt(); break;
		case AIS::KEY_HEADING: v.heading = p.Get().getInt(); break;
		case AIS::KEY_DRAUGHT: if (p.Get().getFloat() != 0.0) v.draught = p.Get().getFloat(); break;
		case AIS::KEY_COURSE: v.cog = p.Get().getFloat(); break;
		case AIS::KEY_SPEED: if ((msg->type() == 9 && p.Get().getInt() != 1023) || (p.Get().getFloat() != 102.3)) v.speed = p.Get().getFloat(); break;
		case AIS::KEY_STATUS: v.status = p.Get().getInt(); break;
		// Dimension Group
		case AIS::KEY_TO_BOW: v.to_bow = p.Get().getInt(); break;
		case AIS::KEY_TO_STERN: v.to_stern = p.Get().getInt(); break;
		case AIS::KEY_TO_PORT: v.to_port = p.Get().getInt(); break;
		case AIS::KEY_TO_STARBOARD: v.to_starboard = p.Get().getInt(); break;
		// Date and Time Group
		case AIS::KEY_MONTH: if (msg->type() == 5) v.month = p.Get().getInt(); break;
		case AIS::KEY_DAY: if (msg->type() == 5) v.day = p.Get().getInt(); break;
		case AIS::KEY_MINUTE: if (msg->type() == 5) v.minute = p.Get().getInt(); break;
		case AIS::KEY_HOUR: if (msg->type() == 5) v.hour = p.Get().getInt(); break;
		// Name Group
		case AIS::KEY_NAME:
		case AIS::KEY_SHIPNAME: std::strncpy(v.shipname, p.Get().getString().c_str(), 20); break;
		case AIS::KEY_CALLSIGN: std::strncpy(v.callsign, p.Get().getString().c_str(), 7); break;
		// Miscellaneous Group
		case AIS::KEY_COUNTRY_CODE: std::strncpy(v.country_code, p.Get().getString().c_str(), 2); break;
		case AIS::KEY_DESTINATION: std::strncpy(v.destination, p.Get().getString().c_str(), 20); break;
		case AIS::KEY_VIRTUAL_AID: v.virtual_aid = p.Get().getBool(); break;
	}
	return position_updated;
}

bool DB::updateShip(const JSON::JSON& data, TAG& tag, VesselDetail& ship) {
	const AIS::Message* msg = (AIS::Message*)data.binary;

	// determine whether we accept msg 27 to update lat/lon
	bool allowApproxLatLon = false, positionUpdated = false;

	if (msg->type() == 27) {
		int timeout = 10 * 60;

		if (ship.speed != SPEED_UNDEFINED && ship.speed != 0)
			timeout = MAX(10, MIN(timeout, (int)(0.25f / ship.speed * 3600.0f)));

		if (msg->getRxTimeUnix() - ship.last_signal > timeout)
			allowApproxLatLon = true;
	}

	ship.mmsi = msg->mmsi();
	ship.count++;
	ship.last_signal = msg->getRxTimeUnix();
	ship.ppm = tag.ppm;
	ship.level = tag.level;
	ship.msg_type |= 1 << msg->type();

	if (msg->getChannel() >= 'A' && msg->getChannel() <= 'D')
		ship.channels |= 1 << (msg->getChannel() - 'A');

	for (const auto& p : data.getProperties())
		positionUpdated |= updateFields(p, msg, ship, allowApproxLatLon);

	if (positionUpdated)
		ship.approximate = msg->type() == 27;

	if(msg_save) {
		if(!ship.msg) ship.msg = new std::string();
		ship.msg->clear();
		builder.stringify(data, *ship.msg);
	}
	return positionUpdated;
}

void DB::addValidation(int ptr, TAG& tag, float lat, float lon) {
}

void DB::Receive(const JSON::JSON* data, int len, TAG& tag) {

	const AIS::Message* msg = (AIS::Message*)data[0].binary;
	int type = msg->type();

	if (type < 1 || type > 27 || msg->mmsi() == 0) return;

	int ptr = findShip(msg->mmsi());

	if (ptr == -1)
		ptr = createShip();

	moveShipToFront(ptr);

	VesselDetail& ship = ships[ptr].ship;
	float lat_old = ship.lat;
	float lon_old = ship.lon;

	bool position_updated = updateShip(data[0], tag, ship);
	position_updated &= isValidCoord(ship.lat, ship.lon);

	if (type == 1 || type == 2 || type == 3 || type == 18 || type == 19 || type == 9)
		addToPath(ptr);

	// add check to update only when new lat/lon
	if (position_updated && isValidCoord(lat, lon)) {
		getDistanceAndBearing(lat, lon, ship.lat, ship.lon, ship.distance, ship.angle);

		tag.distance = ship.distance;
		tag.angle = ship.angle;
	}
	else {
		tag.distance = DISTANCE_UNDEFINED;
		tag.angle = ANGLE_UNDEFINED;
	}

	if (position_updated && isValidCoord(lat_old, lon_old)) {
		// flat earth approximation, roughly 10 nmi
		float d = (ship.lat - lat_old) * (ship.lat - lat_old) + (ship.lon - lon_old) * (ship.lon - lon_old);
		tag.validated = d < 0.1675;
		ships[ptr].ship.validated = tag.validated ? 1 : -1;
	}
	else
		tag.validated = false;

	Send(data, len, tag);
}

//add GEOJSON file

std::string DB::getGEOJSONcompact(bool full) {
    std::stringstream ss;

    ss << "{\"type\":\"FeatureCollection\",\"features\":[";

    std::time_t tm = std::time(nullptr);
    int ptr = first;

    std::string delim = "";
    while (ptr != -1) {
        const VesselDetail& ship = ships[ptr].ship;

        if (ship.mmsi != 0 && isValidCoord(ship.lat, ship.lon)) {
            long int delta_time = static_cast<long int>(tm) - static_cast<long int>(ship.last_signal);

            if (!full && delta_time > TIME_HISTORY)
                break;

            ss << delim << "{\"type\":\"Feature\",\"geometry\":{";
            ss << "\"type\":\"Point\",\"coordinates\":[" << ship.lon << comma << ship.lat << "]";
            ss << "},\"properties\":[";

            ss << ship.mmsi << comma;
            ss << ship.level << comma;
            ss << ship.count << comma;
            ss << ship.ppm << comma;

            ss << (ship.approximate ? 1 : 0) << comma;
            ss << ((ship.heading == HEADING_UNDEFINED) ? null_str : std::to_string(ship.heading)) << comma;
            ss << ((ship.cog == COG_UNDEFINED) ? null_str : std::to_string(ship.cog)) << comma;
            ss << ((ship.speed == SPEED_UNDEFINED) ? null_str : std::to_string(ship.speed)) << comma;
            ss << ((ship.to_bow == DIMENSION_UNDEFINED) ? null_str : std::to_string(ship.to_bow)) << comma;
            ss << ((ship.to_stern == DIMENSION_UNDEFINED) ? null_str : std::to_string(ship.to_stern)) << comma;
            ss << ((ship.to_starboard == DIMENSION_UNDEFINED) ? null_str : std::to_string(ship.to_starboard)) << comma;
            ss << ((ship.to_port == DIMENSION_UNDEFINED) ? null_str : std::to_string(ship.to_port)) << comma;
            ss << ship.shiptype << comma;
            ss << ship.validated << comma;
            ss << ship.msg_type << comma;
            ss << ship.channels << comma;
            ss << "\"" << std::string(ship.country_code) << "\"" << comma;
            ss << ship.status << comma;		
            ss << ship.draught << comma;
			
            ss << ((ship.month == ETA_MONTH_UNDEFINED) ? null_str : std::to_string(ship.month)) << comma;
            ss << ((ship.day == ETA_DAY_UNDEFINED) ? null_str : std::to_string(ship.day)) << comma;
            ss << ((ship.hour == ETA_HOUR_UNDEFINED) ? null_str : std::to_string(ship.hour)) << comma;
            ss << ((ship.minute == ETA_MINUTE_UNDEFINED) ? null_str : std::to_string(ship.minute)) << comma;
            ss << ((ship.IMO == IMO_UNDEFINED) ? null_str : std::to_string(ship.IMO)) << comma;

            std::string callsign_json;
            JSON::StringBuilder::stringify(std::string(ship.callsign), callsign_json);
            ss << callsign_json << comma;

            std::string shipname = std::string(ship.shipname) + (ship.virtual_aid ? " [V]" : "");
            std::string shipname_json;
            JSON::StringBuilder::stringify(shipname, shipname_json);
            ss << shipname_json << comma;

            std::string destination_json;
            JSON::StringBuilder::stringify(std::string(ship.destination), destination_json);
            ss << destination_json << comma << delta_time;

            ss << "]}";

            delim = comma;
        }

        ptr = ships[ptr].next;
    }

    ss << "],\"error\":false}\n\n";

    return ss.str();
}

std::string DB::getGEOJSONtiny(bool full) {
    std::stringstream ss;

    ss << "{\"type\":\"FeatureCollection\",\"features\":[";

    std::time_t tm = std::time(nullptr);
    int ptr = first;

    std::string delim = "";
    while (ptr != -1) {
        const VesselDetail& ship = ships[ptr].ship;

        if (ship.mmsi != 0 && isValidCoord(ship.lat, ship.lon)) {
            long int delta_time = static_cast<long int>(tm) - static_cast<long int>(ship.last_signal);

            if (!full && delta_time > TIME_HISTORY)
                break;

            ss << delim << "{\"type\":\"Feature\",\"geometry\":{";
            ss << "\"type\":\"Point\",\"coordinates\":[" << ship.lon << comma << ship.lat << "]";
            ss << "},\"properties\":[";

            ss << ship.mmsi << comma;

            ss << ((ship.heading == HEADING_UNDEFINED) ? null_str : std::to_string(ship.heading)) << comma;
            ss << ((ship.speed == SPEED_UNDEFINED) ? null_str : std::to_string(ship.speed)) << comma;
            ss << ship.shiptype << comma;
            ss << ship.validated << comma;
            ss << ship.msg_type << comma;

            ss << "\"" << std::string(ship.country_code) << "\"" << comma;

            std::string callsign_json;
            JSON::StringBuilder::stringify(std::string(ship.callsign), callsign_json);
            ss << callsign_json << comma;

            std::string shipname = std::string(ship.shipname) + (ship.virtual_aid ? " [V]" : "");
			
            std::string shipname_json;
            JSON::StringBuilder::stringify(shipname, shipname_json);
            ss << shipname_json;
			
            ss << "]}";

            delim = comma;
        }

        ptr = ships[ptr].next;
    }

    ss << "],\"error\":false}\n\n";

    return ss.str();
}

std::string DB::getGEOJSONmini(bool full) {
    std::stringstream ss;
    ss << "[";

    std::time_t tm = time(nullptr);
    int ptr = first;

    std::string delim = "";
    while (ptr != -1) {
        const VesselDetail& ship = ships[ptr].ship;

        if (ship.mmsi > 100 && isValidCoord(ship.lat, ship.lon)) {
            long int delta_time = static_cast<long int>(tm - ship.last_signal);
            if (!full && delta_time > TIME_HISTORY)
                break;

            ss << delim << "{\"type\":\"Feature\",\"geometry\":{";
            ss << "\"type\":\"Point\",\"coordinates\":[" << std::to_string(ship.lon) << comma << std::to_string(ship.lat) << "]";
            ss << "},\"properties\":[";

            ss << ship.mmsi << comma;
            ss << ((ship.heading == HEADING_UNDEFINED) ? null_str : std::to_string(ship.heading)) << comma;
            ss << ship.shiptype;

            ss << "]}";

            delim = comma;
        }
        ptr = ships[ptr].next;
    }

    ss << "]";
    return ss.str();
}

std::string DB::getship2JSON(bool full) {
    std::string str;

    std::stringstream ss;
    ss << "{\"values\":[";

    std::time_t tm = time(nullptr);
    int ptr = first;

    std::string delim = "";
    while (ptr != -1) {
        const VesselDetail ship = ships[ptr].ship;
        if (ship.mmsi >= 100) {
            long int delta_time = (long int)tm - (long int)ship.last_signal;
            if (!full && delta_time > TIME_HISTORY) break;

            std::string jsonShip;
            jsonShip += "[";
            jsonShip += std::to_string(ship.mmsi) + comma;
            jsonShip += ((ship.to_bow == DIMENSION_UNDEFINED) ? null_str : std::to_string(ship.to_bow)) + comma;
            jsonShip += ((ship.to_stern == DIMENSION_UNDEFINED) ? null_str : std::to_string(ship.to_stern)) + comma;
            jsonShip += ((ship.to_starboard == DIMENSION_UNDEFINED) ? null_str : std::to_string(ship.to_starboard)) + comma;
            jsonShip += ((ship.to_port == DIMENSION_UNDEFINED) ? null_str : std::to_string(ship.to_port)) + comma;
            jsonShip += std::to_string(ship.shiptype) + comma;
            jsonShip += "\"" + std::string(ship.country_code) + "\",";
            jsonShip += std::to_string(ship.status) + comma;
            jsonShip += ((ship.IMO == IMO_UNDEFINED) ? null_str : std::to_string(ship.IMO)) + comma;

            str = std::string(ship.callsign);
            JSON::StringBuilder::stringify(str, jsonShip);

            jsonShip += comma;
            str = std::string(ship.shipname) + (ship.virtual_aid ? std::string(" [V]") : std::string(""));
            JSON::StringBuilder::stringify(str, jsonShip);

            jsonShip += "]";
            
            ss << delim << jsonShip;
            delim = comma;
        }
        ptr = ships[ptr].next;
    }

    ss << "]}\n\n";
    return ss.str();
}


std::string DB::getJSONfive(bool full) {
    std::stringstream ss;

    ss << "[";

    std::time_t tm = time(nullptr);
    int ptr = first;

    std::string delim = "";
    while (ptr != -1) {
        const VesselDetail& ship = ships[ptr].ship;
        if (ship.mmsi != 0) {
            long int delta_time = static_cast<long int>(tm) - static_cast<long int>(ship.last_signal);
            if (!full && delta_time > TIME_HISTORY)
                break;

            if (isValidCoord(ship.lat, ship.lon)) {
                ss << delim << "[" << ship.mmsi << comma;
                ss << std::to_string(ship.lat) << comma;
                ss << std::to_string(ship.lon) << comma;
                ss << (ship.heading == HEADING_UNDEFINED ? null_str : std::to_string(ship.heading)) << comma;
                ss << ship.shiptype << "]";
                delim = comma;
            }
        }
        ptr = ships[ptr].next;
    }

    ss << "]";

    return ss.str();
}

// moving_ship_layer with 5 elements : mmsi, lat, lon, heading, type

std::string DB::getJSON6SHIP_M(bool full) { 
    std::stringstream ss;

    std::time_t tm = time(nullptr);
    int ptr = first;

    int shipCount = 0; // Initialize ship count variable
    std::string delim = "";

    ss << "["; // Start of the "ships" array

    while (ptr != -1) {
        const VesselDetail& ship = ships[ptr].ship;
        if (getIconID(ship.mmsi) == 0 && (ship.heading != HEADING_UNDEFINED) && (ship.speed > 0)) {
            long int delta_time = static_cast<long int>(tm) - static_cast<long int>(ship.last_signal);
            if (!full && delta_time > TIME_HISTORY)
                break;

            if (isValidCoord(ship.lat, ship.lon)) {
                ss << delim << "[" << ship.mmsi << comma;
                ss << std::to_string(ship.lat) << comma;
                ss << std::to_string(ship.lon) << comma;
                ss << std::to_string(ship.heading) << comma;
                ss << ship.shiptype << "]";
                delim = comma;

                shipCount++; // Increment ship count
            }
        }
        ptr = ships[ptr].next;
    }

    ss << "]"; // End of the "ships" array

    // Create the output JSON object
    std::stringstream result;
    result << "{\"shipCount\":" << shipCount << ",";
    result << "\"ships\":" << ss.str() << "}";

    return result.str();
}

// moving_ship_layer with 4 elements : mmsi, lat, lon, type

std::string DB::getJSON5SHIP_A(bool full) { // anhchoring_ship_layer
    std::stringstream ss;

    std::time_t tm = time(nullptr);
    int ptr = first;

    int shipACount = 0; // Initialize ship count variable
    std::string delim = "";

    ss << "["; // Start of the "ships" array

    while (ptr != -1) {
        const VesselDetail& ship = ships[ptr].ship;
        if (getIconID(ship.mmsi) == 0 && (ship.heading == HEADING_UNDEFINED)) {
            long int delta_time = static_cast<long int>(tm) - static_cast<long int>(ship.last_signal);
            if (!full && delta_time > TIME_HISTORY)
                break;

            if (isValidCoord(ship.lat, ship.lon)) {
                ss << delim << "[" << ship.mmsi << comma;
                ss << std::to_string(ship.lat) << comma;
                ss << std::to_string(ship.lon) << comma;
                ss << ship.shiptype << "]";
                delim = comma;

                shipACount++; // Increment ship count
            }
        }
        ptr = ships[ptr].next;
    }

    ss << "]"; // End of the "ships" array

    // Create the output JSON object
    std::stringstream result;
    result << "{\"shipACount\":" << shipACount << ",";
    result << "\"ships\":" << ss.str() << "}";

    return result.str();
}

std::string DB::getJSON_STATION(bool full) { // anhchoring_ship_layer
    std::stringstream ss;

    std::time_t tm = time(nullptr);
    int ptr = first;

    int stationCount = 0; // Initialize ship count variable
    std::string delim = "";

    ss << "["; // Start of the "ships" array

    while (ptr != -1) {
        const VesselDetail& ship = ships[ptr].ship;
        if (getIconID(ship.mmsi) == 1) {
            long int delta_time = static_cast<long int>(tm) - static_cast<long int>(ship.last_signal);
            if (!full && delta_time > TIME_HISTORY)
                break;

            if (isValidCoord(ship.lat, ship.lon)) {
                ss << delim << "[" << ship.mmsi << comma;
                ss << std::to_string(ship.lat) << comma;
                ss << std::to_string(ship.lon) << "]";
                delim = comma;

                stationCount++; // Increment ship count
            }
        }
        ptr = ships[ptr].next;
    }

    ss << "]"; // End of the "ships" array

    // Create the output JSON object
    std::stringstream result;
    result << "{\"stationCount\":" << stationCount << ",";
    result << "\"ships\":" << ss.str() << "}";

    return result.str();
}

// ATON with 4 elements : mmsi, lat, lon, iconID

std::string DB::getJSON_ATON(bool full) { // anhchoring_ship_layer
    std::stringstream ss;

    std::time_t tm = time(nullptr);
    int ptr = first;

    int ATONCount = 0; // Initialize ship count variable
    std::string delim = "";

    ss << "["; // Start of the "ships" array

    while (ptr != -1) {
        const VesselDetail& ship = ships[ptr].ship;
        if (getIconID(ship.mmsi) == 2) {
            long int delta_time = static_cast<long int>(tm) - static_cast<long int>(ship.last_signal);
            if (!full && delta_time > TIME_HISTORY)
                break;

            if (isValidCoord(ship.lat, ship.lon)) {
                ss << delim << "[" << ship.mmsi << comma;
                ss << std::to_string(ship.lat) << comma;
                ss << std::to_string(ship.lon) << "]";
                delim = comma;

                ATONCount++; // Increment ship count
            }
        }
        ptr = ships[ptr].next;
    }

    ss << "]"; // End of the "ships" array

    // Create the output JSON object
    std::stringstream result;
    result << "{\"ATONCount\":" << ATONCount << ",";
    result << "\"ships\":" << ss.str() << "}";

    return result.str();
}

// SAR with 4 elements : mmsi, lat, lon, iconID

std::string DB::getJSON_SAR(bool full) {
    std::stringstream ss;

    std::time_t tm = time(nullptr);
    int ptr = first;

    int SARCount = 0; // Initialize ship count variable
    std::string delim = "";

    ss << "["; // Start of the "ships" array

    while (ptr != -1) {
        const VesselDetail& ship = ships[ptr].ship;
        if (getIconID(ship.mmsi) == 3) {
            long int delta_time = static_cast<long int>(tm) - static_cast<long int>(ship.last_signal);
            if (!full && delta_time > TIME_HISTORY)
                break;

            if (isValidCoord(ship.lat, ship.lon)) {
                ss << delim << "[" << ship.mmsi << comma;
                ss << std::to_string(ship.lat) << comma;
                ss << std::to_string(ship.lon) << "]";
                delim = comma;

                SARCount++; // Increment ship count
            }
        }
        ptr = ships[ptr].next;
    }

    ss << "]"; // End of the "ships" array

    // Create the output JSON object
    std::stringstream result;
    result << "{\"SARCount\":" << SARCount << ",";
    result << "\"ships\":" << ss.str() << "}";

    return result.str();
}

// ShipGroup with 4 elements : mmsi, lat, lon, iconID

std::string DB::getJSON_SG(bool full) {
    std::stringstream ss;

    std::time_t tm = time(nullptr);
    int ptr = first;

    int SGCount = 0; // Initialize ship count variable
    std::string delim = "";

    ss << "["; // Start of the "ships" array

    while (ptr != -1) {
        const VesselDetail& ship = ships[ptr].ship;
        if (getIconID(ship.mmsi) == 4) {
            long int delta_time = static_cast<long int>(tm) - static_cast<long int>(ship.last_signal);
            if (!full && delta_time > TIME_HISTORY)
                break;

            if (isValidCoord(ship.lat, ship.lon)) {
                ss << delim << "[" << ship.mmsi << comma;
                ss << std::to_string(ship.lat) << comma;
                ss << std::to_string(ship.lon) << comma;
				ss << std::to_string(ship.heading) << comma;
                ss << ship.shiptype << "]";;
                delim = comma;

                SGCount++; // Increment ship count
            }
        }
        ptr = ships[ptr].next;
    }

    ss << "]"; // End of the "ships" array

    // Create the output JSON object
    std::stringstream result;
    result << "{\"SGCount\":" << SGCount << ",";
    result << "\"ships\":" << ss.str() << "}";

    return result.str();
}

std::string DB::getJSON_AIS_SART(bool full) {
    std::stringstream ss;

    std::time_t tm = time(nullptr);
    int ptr = first;

    int SARTCount = 0; // Initialize ship count variable
    std::string delim = "";

    ss << "["; // Start of the "ships" array

    while (ptr != -1) {
        const VesselDetail& ship = ships[ptr].ship;
        if (getIconID(ship.mmsi) == 5) {
            long int delta_time = static_cast<long int>(tm) - static_cast<long int>(ship.last_signal);
            if (!full && delta_time > TIME_HISTORY)
                break;

            if (isValidCoord(ship.lat, ship.lon)) {
                ss << delim << "[" << ship.mmsi << comma;
                ss << std::to_string(ship.lat) << comma;
                ss << std::to_string(ship.lon) << "]";
                delim = comma;

                SARTCount++; // Increment ship count
            }
        }
        ptr = ships[ptr].next;
    }

    ss << "]"; // End of the "ships" array

    // Create the output JSON object
    std::stringstream result;
    result << "{\"SARTCount\":" << SARTCount << ",";
    result << "\"ships\":" << ss.str() << "}";

    return result.str();
}

std::string DB::getJSON_USO(bool full) {
    std::stringstream ss;

    std::time_t tm = time(nullptr);
    int ptr = first;

    int USOCount = 0; // Initialize ship count variable
    std::string delim = "";

    ss << "["; // Start of the "ships" array

    while (ptr != -1) {
        const VesselDetail& ship = ships[ptr].ship;
        if (getIconID(ship.mmsi) == 6) {
            long int delta_time = static_cast<long int>(tm) - static_cast<long int>(ship.last_signal);
            if (!full && delta_time > TIME_HISTORY)
                break;

            if (isValidCoord(ship.lat, ship.lon)) {
                ss << delim << "[" << ship.mmsi << comma;
                ss << std::to_string(ship.lat) << comma;
                ss << std::to_string(ship.lon) << "]";
                delim = comma;

                USOCount++; // Increment ship count
            }
        }
        ptr = ships[ptr].next;
    }

    ss << "]"; // End of the "ships" array

    // Create the output JSON object
    std::stringstream result;
    result << "{\"USOCount\":" << USOCount << ",";
    result << "\"ships\":" << ss.str() << "}";

    return result.str();
}