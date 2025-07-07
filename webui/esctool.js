var localStorageName = "esctool";

const vendorIdInputID			= "vendor-id-input";
const vendorNameInputID			= "vendor-name-input";
const groupSelectorID			= "group-selector";
const devViewDeviceNameID		= "dv-device-name";
const devViewDeviceProductCodeID	= "dv-product-code";
const devViewDevicRevisionNoID		= "dv-revision-no";
const devViewDevicEepromByteSizeID	= "dv-eeprom-bytesize";

const devViewDevicPdiControlID		= "dv-pdi-control";
const devViewDevicPdiConfigurationID	= "dv-pdi-conf";
const devViewDevicPdiConfiguration2ID	= "dv-pdi-conf2";
const devViewSyncImpulseLenID		= "dv-sync-impulse-len";
const devConfiguredStationAliasID	= "dv-configured-station-alias";

const variableStr	= "VARIABLE";
const recordStr		= "RECORD";
const arrayStr		= "ARRAY";

const selectedSetup = 0;
var currentDevice = null;

var setup = {
	setups: [ {
			setupname: "default",
			vendorname: "",
			vendorid: "",
			groups: []
		}
	]
};

const readOnlyStr = "ro";
const readWriteStr = "rw";
const writeOnlyStr = "wo";

var syncManagerSetup = {
	type: [ 0x1, 0x2, 0x3, 0x4],
	rxpdo: 0x2,
	txpdo: 0x3,
	conf: [  {
			MinSize: 0x24,
			MaxSize: 0x80,
			DefaultSize: 0x80,
			StartAddress: 0x1000,
			ControlByte: 0x26,
			Enable: 1,
			Description: "MBoxOut"
		},
		{
			MinSize: 0x24,
			MaxSize: 0x80,
			DefaultSize: 0x80,
			StartAddress: 0x1080,
			ControlByte: 0x22,
			Enable: 1,
			Description: "MBoxIn"
		},
		{
			DefaultSize: 0x0,
			StartAddress: 0x1100,
			ControlByte: 0x64,
			Enable: 1,
			Description: "Outputs"
		},
		{
			DefaultSize: 0x0,
			StartAddress: 0x1400,
			ControlByte: 0x20,
			Enable: 1,
			Description: "Inputs"
		}
	]
};

var mailboxSetup = {
	DataLinkLayer: true,
	CoE: {
		SdoInfo: true,
		PdoAssign: false,
		PdoConfig: false,
		CompleteAccess: true,
		SegmentedSdo: true
	},
	FoE: false
};

var dcSetup = {
	opModes : [
		{
			Name: "Synchron",
			Desc: "SM-Synchron",
			AssignActivate: 0
		},
		{
			Name: "DC",
			Desc: "DC-Synchron",
			AssignActivate: 0x300,
			CycleTymeSync0: {
				value: 0,
				Factor: 1
			},
			CycleTymeSync1: {
				value: 0,
				Factor: 1
			},
		},

	]
};

var eeprom = {
	ByteSize: 2048,
	ConfigData: "06800681000000000000"
}

const configDataCtrlBegin = 0;
const configDataConfBegin = 4;
const configDataConf2Begin = 12;
const configDataSyncImpBegin = 8;
const configDataConfStationAliasBegin = 16;

var dataTypes = [];

const pdoTXVal = "tx";
const pdoRXVal = "rx";

var pdoTable = [
	{ type: pdoRXVal, index: 0x1600, length: 0x200, bitLength: 0 },
	{ type: pdoTXVal, index: 0x1A00, length: 0x200, bitLength: 0 }
];

var addrs = [
	{ begin: 0x2000, length: 0x3fff, label: "Manufacturer specific area" },
	{ begin: 0x6000, length: 0xfff, label: "Input area" },
	{ begin: 0x7000, length: 0xfff, label: "Output area" }
];

/**
 * 
 * @param {number} numberDec 
 * @param {number} leadingZeros 
 * @returns 
 */
function formatHex(numberDec,leadingZeros) {
	if(isNaN(numberDec)) numberDec = 0;
	return "0x"+("00000000"+numberDec.toString(16).toUpperCase()).slice(-leadingZeros);
}

/**
 * 
 * @param {number} numberDec 
 * @param {number} leadingZeros 
 * @returns 
 */
function formatHexRaw(numberDec,leadingZeros) {
	if(isNaN(numberDec)) numberDec = 0;
	return ("00000000"+numberDec.toString(16).toUpperCase()).slice(-leadingZeros);
}

/**
 * 
 * @param {number} numberDec 
 * @param {number} leadingZeros 
 * @returns 
 */
function formatDec(numberDec,leadingZeros) {
	if(isNaN(numberDec)) numberDec = 0;
	return ("00000000"+numberDec).slice(-leadingZeros);
}

function swap32(val) {
	return ((val & 0xFF) << 24)
	       | ((val & 0xFF00) << 8)
	       | ((val >> 8) & 0xFF00)
	       | ((val >> 24) & 0xFF);
}

function writeConsole(str) {
	let consoleDiv = document.getElementById('Console');
	if(consoleDiv) consoleDiv.innerHTML = str;
}

function loadSetup(currentSetup) {
	let vendorname = document.getElementById(vendorNameInputID);
	if(vendorname !== null) {
		vendorname.value = currentSetup.vendorname;
	}

	let vendorid = document.getElementById(vendorIdInputID);
	if(vendorid !== null) {
		vendorid.value = currentSetup.vendorid;
	}

	let groupSelector = document.getElementById(groupSelectorID);
	if(groupSelector !== null) {
		currentSetup.groups.forEach((val, index, arr) =>{
			addGroupOption(groupSelector,val);
		});
		groupSelector.selectedIndex = 0;
		groupSelector.dispatchEvent(new Event('change')); // Fire change event, to update deviceSelector
	}
	console.log(currentSetup);
	writeConsole("Loaded setup \""+currentSetup.setupname+"\"");
}

function loadSetups() {
	let xhr = new XMLHttpRequest;
	xhr.open("GET", "/setup", true);
	xhr.setRequestHeader("Content-Type", "application/json");
	xhr.onreadystatechange = () => {
		if (xhr.readyState === XMLHttpRequest.DONE && xhr.status === 200) {
			console.log("Loaded setup from disk.");
			setup = JSON.parse(xhr.responseText);
			loadSetup(setup.setups[selectedSetup]);
		}
	};
	xhr.send();
}

function saveSetup() {
//	localStorage.setItem(localStorageName,JSON.stringify(setup));
	let xhr = new XMLHttpRequest();

	xhr.open("POST", "/setup", true);
	xhr.setRequestHeader("Content-Type", "application/json");
	xhr.onreadystatechange = () => {
		if (xhr.readyState === XMLHttpRequest.DONE && xhr.status === 200) {
			console.log("Saved setup to disk.");
		}
	};
	xhr.send(JSON.stringify(setup,null,2));

}

function initDevice(device) {
	if(device.syncManagerSetup === undefined) device.syncManagerSetup = structuredClone(syncManagerSetup);
	if(device.dcSetup === undefined) device.dcSetup = structuredClone(dcSetup);
	if(device.mailboxSetup === undefined) device.mailboxSetup = structuredClone(mailboxSetup);
	if(device.eeprom === undefined) device.eeprom = structuredClone(eeprom);
	return device;
}

function loadDevice(device) {
	// Remove any unindexed objects that might exist
	if(device.objects !== undefined && device.objects !== null) {
		device.objects.forEach((val,index,arr) => {
			if(val.index === undefined || val.index === null || val.index == 0 || val.index == "") {
				device.objects.splice(index,1);
			}
		});
	}

	currentDevice = initDevice(device);

	let dvDeviceName = document.getElementById(devViewDeviceNameID);
	if(dvDeviceName !== undefined) {
		dvDeviceName.value = device.name;
	}

	let dvDeviceProductCode = document.getElementById(devViewDeviceProductCodeID);
	if(dvDeviceProductCode !== undefined) {
		dvDeviceProductCode.value = device.productCode !== undefined ? device.productCode : "";
	}

	let dvDeviceRevisionNo = document.getElementById(devViewDevicRevisionNoID);
	if(dvDeviceRevisionNo !== undefined) {
		dvDeviceRevisionNo.value = device.revisionNo !== undefined ? device.revisionNo : "";
	}

	let dvDeviceEepromByteSize = document.getElementById(devViewDevicEepromByteSizeID);
	if(dvDeviceEepromByteSize !== undefined) {
		dvDeviceEepromByteSize.value = device.eeprom.ByteSize !== undefined ? device.eeprom.ByteSize : "";
	}

	let devViewDevicPdiControl = document.getElementById(devViewDevicPdiControlID);
	if(devViewDevicPdiControl !== undefined) {
		if(device.eeprom.ConfigData != "") devViewDevicPdiControl.value =
			formatHex(parseInt(device.eeprom.ConfigData.substring(configDataCtrlBegin,configDataCtrlBegin+4),16),4);
		else {
			devViewDevicPdiControl.value = formatHex(parseInt(eeprom.ConfigData.substring(configDataCtrlBegin,configDataCtrlBegin+4),16),4);
			devViewDevicPdiControl.dispatchEvent(new Event('change'));
		}
	}

	let devViewDevicPdiConfiguration = document.getElementById(devViewDevicPdiConfigurationID);
	if(devViewDevicPdiConfiguration !== undefined) {
		if(device.eeprom.ConfigData != "") devViewDevicPdiConfiguration.value =
			formatHex(parseInt(device.eeprom.ConfigData.substring(configDataConfBegin,configDataConfBegin+4),16),4);
		else {
			devViewDevicPdiConfiguration.value = formatHex(parseInt(eeprom.ConfigData.substring(configDataConfBegin,configDataConfBegin+4),16),4);
			devViewDevicPdiConfiguration.dispatchEvent(new Event('change'));
		}
	}

	let devViewDevicPdiConfiguration2 = document.getElementById(devViewDevicPdiConfiguration2ID);
	if(devViewDevicPdiConfiguration2 !== undefined) {
		if(device.eeprom.ConfigData != "") devViewDevicPdiConfiguration2.value =
			formatHex(parseInt(device.eeprom.ConfigData.substring(configDataConf2Begin,configDataConf2Begin+4),16),4);
		else {
			devViewDevicPdiConfiguration2.value = formatHex(parseInt(eeprom.ConfigData.substring(configDataConf2Begin,configDataConf2Begin+4),16),4);
			devViewDevicPdiConfiguration2.dispatchEvent(new Event('change'));
		}
	}

	let devViewSyncImpulseLen = document.getElementById(devViewSyncImpulseLenID);
	if(devViewSyncImpulseLen !== undefined) {
		if(device.eeprom.ConfigData != "") devViewSyncImpulseLen.value =
			formatHex(parseInt(device.eeprom.ConfigData.substring(configDataSyncImpBegin,configDataSyncImpBegin+4),16),4);
		else {
			devViewSyncImpulseLen.value = formatHex(parseInt(eeprom.ConfigData.substring(configDataSyncImpBegin,configDataSyncImpBegin+4),16),4);
			devViewSyncImpulseLen.dispatchEvent(new Event('change'));
		}
	}

	let devConfiguredStationAlias = document.getElementById(devConfiguredStationAliasID);
	if(devConfiguredStationAlias !== undefined) {
		if(device.eeprom.ConfigData != "") devConfiguredStationAlias.value =
			formatHex(parseInt(device.eeprom.ConfigData.substring(configDataConfStationAliasBegin,configDataConfStationAliasBegin+4),16),4);
		else {
			devConfiguredStationAlias.value = formatHex(parseInt(eeprom.ConfigData.substring(configDataConfStationAliasBegin,configDataConfStationAliasBegin+4),16),4);
			devConfiguredStationAlias.dispatchEvent(new Event('change'));
		}
	}

	// Remove any currently allocated objects in the UI
	addrs.forEach((addr) => {
		let container = document.getElementById("addr"+formatHexRaw(addr.begin,4));
		if(container.objects !== undefined) {
			container.objects.forEach((obj) => { obj.remove(); });
			container.objects = [];
		}
	});

	// Add objects to the UI in the respective address spaces
	addrs.forEach((addr) => {
		let addrEnd = addr.begin+addr.length;
		if (device.objects !== undefined && device.objects != null) {
			device.objects.forEach((obj,index,arr) => {
				let objIndex = parseInt(obj.index);
				if(objIndex >= addr.begin && objIndex <= addrEnd) {
					let container = document.getElementById("addr"+formatHexRaw(addr.begin,4));
					if(container !== undefined && container != null) {
						addObject(container,obj);
					}
				}
			});
		}
	});

	currentDevice.profileNo = "5001"; // TODO: not hardcode
}

function selectIndexWithValue(selector,value) {
	for(let i = 0; i < selector.options.length; ++i) {
		if(selector.options.item(i).value == value) {
			selector.selectedIndex = i;
			// fire change event?
			selector.dispatchEvent(new Event('change'));
			break;
		}
	}
}

function addSubIndex(container,index,subIndexNo) {
	let removeSubIndex = function(subidx) {
		container.subitems[subidx].remove();
		container.subitems.splice(container.subitems[subidx].subIndexNo,1);
		for(let i = 0; i < container.subitems.length; ++i) {
			container.subitems[i].subIndexNo = i;
			container.subitems[i].subIndexInputField.value = formatHex(i,2);
		}
		container.object.subitems.splice(subidx,1);
		if(container.typeSelector.value == arrayStr ||
		   container.typeSelector.value == recordStr)
		{
			container.objectDefaultDataInputField.value =
				formatHex(container.object.subitems.length-1,2);
			container.objectDefaultDataInputField.dispatchEvent(new Event('change'));
		}
	};

	let newSubIndex = document.createElement("div");
	newSubIndex.className = "object-subindex-div";

	let addrInputField = document.createElement("input");
	addrInputField.title = "Object address";
	addrInputField.className = "object-address-input";
	addrInputField.onchange = (event) => {
		let newVal = parseInt(addrInputField.value,16);
		addrInputField.value = formatHex(newVal,4);
		container.object.index = addrInputField.value;
	};

	container.object.subitems[subIndexNo].subIdx = subIndexNo;

	if(subIndexNo > 0) {
		addrInputField.disabled = true;
	} else {
		addrInputField.value = formatHex(index,4);
		container.object.index = addrInputField.value;
	}

	let populateSelector = (selector,elements) => {
		elements.forEach((value,index,entry) => {
			let opt = document.createElement("option");
			opt.label = entry[index][0];
			opt.value = entry[index][1];
			selector.appendChild(opt);
		});
	};

	let typeSelector = document.createElement("select");
	typeSelector.title = "Object type";
	typeSelector.className = "object-type-selector";

	let typeOptions = [
		["Record",recordStr],
		["Variable",variableStr],
		["Array",arrayStr]
	];

	let subIndexInputField = document.createElement("input");
	subIndexInputField.title = "Subindex";
	subIndexInputField.className = "object-subindex-input";
	subIndexInputField.value = formatHex(subIndexNo,2);
	subIndexInputField.onchange = (event) => {
		let newVal = parseInt(subIndexInputField.value,16);
		subIndexInputField.value = formatHex(newVal,2);
	};

	let subIndexDataTypeInputField = document.createElement("input");
	subIndexDataTypeInputField.title = "DataType";
	subIndexDataTypeInputField.className = "object-datatype-input";

	subIndexDataTypeInputField.onchange = (event) => {
		container.object.subitems[subIndexNo].datatype = subIndexDataTypeInputField.value;
	};

	if(container.object.subitems !== undefined &&
	   container.object.subitems[subIndexNo] !== undefined &&
	   container.object.subitems[subIndexNo].datatype !== undefined)
	{
		subIndexDataTypeInputField.value = container.object.subitems[subIndexNo].datatype;
	} else if(subIndexNo == 0) {
		subIndexDataTypeInputField.value = "USINT";
		subIndexDataTypeInputField.dispatchEvent(new Event('change')); // Fire change event, to update subitem
	}

	let objectNameInputField = document.createElement("input");
	objectNameInputField.title = "Name";
	objectNameInputField.className = "object-name-input";

	if(container.object.subitems !== undefined &&
	   container.object.subitems[subIndexNo] !== undefined &&
	   container.object.subitems[subIndexNo].name !== undefined)
	{
		objectNameInputField.value = container.object.subitems[subIndexNo].name;
	}

	objectNameInputField.onchange = (event) => {
		container.object.subitems[subIndexNo].name = objectNameInputField.value;
	};

	let objectDefaultDataInputField = document.createElement("input");
	objectDefaultDataInputField.title = "Default data";
	objectDefaultDataInputField.className = "object-default-data-input";

	if(container.object.subitems !== undefined &&
		container.object.subitems[subIndexNo] !== undefined &&
		container.object.subitems[subIndexNo].defaultData !== undefined)
	{
		objectDefaultDataInputField.value = container.object.subitems[subIndexNo].defaultData;
	}

	objectDefaultDataInputField.onchange = (event) => {
		container.object.subitems[subIndexNo].defaultData = objectDefaultDataInputField.value;
	};

	let accessTypeSelector = document.createElement("select");
	accessTypeSelector.title = "Access";
	accessTypeSelector.className = "object-access-selector"

	let readRestrictionsSelector = document.createElement("select");
	readRestrictionsSelector.title = "Read restrictions";
	readRestrictionsSelector.className = "object-wr-restrictions-selector"

	let writeRestrictionsSelector = document.createElement("select");
	writeRestrictionsSelector.title = "Write restrictions";
	writeRestrictionsSelector.className = "object-wr-restrictions-selector"

	accessTypeSelector.onchange = (event) => {
		if(accessTypeSelector.selectedOptions[0].value.indexOf("r") != -1) {
			readRestrictionsSelector.disabled = false;
		} else {
			readRestrictionsSelector.selectedIndex = 0;
			readRestrictionsSelector.disabled = true;
		}
		if(accessTypeSelector.selectedOptions[0].value.indexOf("w") != -1) {
			writeRestrictionsSelector.disabled = false;
		} else {
			writeRestrictionsSelector.selectedIndex = 0;
			writeRestrictionsSelector.disabled = true;
		}
		container.object.subitems[subIndexNo].access = accessTypeSelector.value;
	};

	let accessOpts = [
		["Read Only",readOnlyStr],
		["Read/Write",readWriteStr],
		["Write only",writeOnlyStr]
	];

	populateSelector(accessTypeSelector,accessOpts);

	let wrRestrictions = [
		["(n/a)",""],
		["Pre-Op","PreOP"],
		["Pre-Op/Safe-Op","PreOP_SafeOP"],
		["Pre-Op/Op","PreOP_OP"],
		["Safe-Op","SafeOP"],
		["Safe-Op/Op","SafeOP_OP"],
		["Op","OP"]
	];

	populateSelector(readRestrictionsSelector,wrRestrictions);
	populateSelector(writeRestrictionsSelector,wrRestrictions);

	if(container.object.subitems !== undefined &&
		container.object.subitems[subIndexNo] !== undefined &&
		container.object.subitems[subIndexNo].access !== undefined)
	{
		selectIndexWithValue(accessTypeSelector,container.object.subitems[subIndexNo].access);
	} else selectIndexWithValue(accessTypeSelector,readOnlyStr);

	if(container.object.subitems !== undefined &&
		container.object.subitems[subIndexNo] !== undefined &&
		container.object.subitems[subIndexNo].readRestrictions !== undefined)
	{
		selectIndexWithValue(readRestrictionsSelector,container.object.subitems[subIndexNo].readRestrictions);
	}

	readRestrictionsSelector.onchange = (event) => {
		container.object.subitems[subIndexNo].readRestrictions = readRestrictionsSelector.value;
	};

	if(container.object.subitems !== undefined &&
		container.object.subitems[subIndexNo] !== undefined &&
		container.object.subitems[subIndexNo].writeRestrictions !== undefined)
	{
		selectIndexWithValue(writeRestrictionsSelector,container.object.subitems[subIndexNo].writeRestrictions);
	}

	writeRestrictionsSelector.onchange = (event) => {
		container.object.subitems[subIndexNo].writeRestrictions = writeRestrictionsSelector.value;
	};

	let pdoOpts = [
		["n/a",""],
		["RX",pdoRXVal],
		["TX",pdoTXVal]
	];

	let pdoSelector = document.createElement("select");
	pdoSelector.title = "PDO selection";
	pdoSelector.className = "object-pdo-selector"

	populateSelector(pdoSelector,pdoOpts);

	if(container.object.subitems !== undefined &&
		container.object.subitems[subIndexNo] !== undefined &&
		container.object.subitems[subIndexNo].pdo !== undefined)
	{
		selectIndexWithValue(pdoSelector,container.object.subitems[subIndexNo].pdo);
	}

	pdoSelector.onchange = (event) => {
		container.object.subitems[subIndexNo].pdo = pdoSelector.value;
	};

	newSubIndex.appendChild(addrInputField);
	newSubIndex.appendChild(typeSelector);
	newSubIndex.appendChild(subIndexInputField);
	newSubIndex.appendChild(subIndexDataTypeInputField);
	newSubIndex.appendChild(objectNameInputField);
	newSubIndex.appendChild(objectDefaultDataInputField);
	newSubIndex.appendChild(accessTypeSelector);
	newSubIndex.appendChild(readRestrictionsSelector);
	newSubIndex.appendChild(writeRestrictionsSelector);
	newSubIndex.appendChild(pdoSelector);

	newSubIndex.addrInputField = addrInputField;
	newSubIndex.subIndexNo = subIndexNo;
	newSubIndex.subIndexInputField = subIndexInputField;
	newSubIndex.subIndexDataTypeInputField = subIndexDataTypeInputField;

	if(subIndexNo == 0) {
		let addSubIndexButton = document.createElement('button');
		addSubIndexButton.className = "subindex-add-button";
		addSubIndexButton.title = "Add subindex";
		addSubIndexButton.innerHTML = "+";

		addSubIndexButton.onclick = (event) => {
			let lastSI = container.subitems[container.subitems.length-1].subIndexNo;
			container.object.subitems.push({});
			container.appendChild(addSubIndex(container,index,lastSI+1));
			if(typeSelector.value == arrayStr || typeSelector.value == recordStr) {
				objectDefaultDataInputField.value = formatHex(container.object.subitems.length-1,2);
				objectDefaultDataInputField.dispatchEvent(new Event('change'));
			}
		};
		container.typeSelector = typeSelector;
		container.addSubIndexButton = addSubIndexButton;
		container.objectDefaultDataInputField = objectDefaultDataInputField;
		newSubIndex.appendChild(addSubIndexButton);
	} else {
		let deleteSubIndexButton = document.createElement('button');
		deleteSubIndexButton.className = "subindex-delete-button";
		deleteSubIndexButton.title = "Delete subindex";
		deleteSubIndexButton.innerHTML = "-";

		deleteSubIndexButton.onclick = (event) => {
			removeSubIndex(newSubIndex.subIndexNo);
		};
		newSubIndex.appendChild(deleteSubIndexButton);
	}

	if(subIndexNo == 0) {
		let deleteObjectButton = document.createElement("button");
		deleteObjectButton.className = "object-delete-button";
		deleteObjectButton.title = "Delete object";
		deleteObjectButton.innerHTML = "-";

		deleteObjectButton.onclick = (event) => {
			currentDevice.objects.every((val,index,arr) => {
				if(val == container.object) {
					currentDevice.objects.splice(index,1);
					return false;
				}
				return true;
			});
			container.parentElement.objects.every((val,index) => {
				if(val == container) {
					container.parentElement.objects.splice(index,1);
					return false;
				}
				return true;
			});
			container.remove();
		};

		newSubIndex.appendChild(deleteObjectButton);
	}

	if(subIndexNo > 0) {
		typeSelector.disabled = true;
		if(container.typeSelector.value == arrayStr) {
			subIndexDataTypeInputField.disabled = true;
		}
	} else {
		populateSelector(typeSelector,typeOptions);
		container.object.type = typeSelector.options.item(0).value;
		typeSelector.onchange = (event) => {
			container.object.type = typeSelector.value;
			if(typeSelector.value == variableStr) {
				subIndexInputField.disabled = true;
				container.addSubIndexButton.disabled = true;
				while(container.subitems.length > 1)
					removeSubIndex(container.subitems.length-1);
				objectDefaultDataInputField.disabled = false;
			} else if(typeSelector.value == recordStr) {
				subIndexInputField.disabled = true;
				container.addSubIndexButton.disabled = false;
				objectDefaultDataInputField.disabled = true;
				objectDefaultDataInputField.value = formatHex(container.object.subitems.length-1,2);
				objectDefaultDataInputField.dispatchEvent(new Event('change'));
				container.subitems.every((val,index) => {
					if(0 != index) val.subIndexDataTypeInputField.disabled = false;
					return true;
				});
			} else if(typeSelector.value == arrayStr) {
				subIndexInputField.disabled = true;
				container.addSubIndexButton.disabled = false;
				objectDefaultDataInputField.disabled = true;
				objectDefaultDataInputField.value = formatHex(container.object.subitems.length-1,2);
				objectDefaultDataInputField.dispatchEvent(new Event('change'));
				container.subitems.every((val,index) => {
					if(0 != index) val.subIndexDataTypeInputField.disabled = true;
					return true;
				});
			}
		}
		if(container.object.type !== undefined) {
			selectIndexWithValue(typeSelector,container.object.type);
		}
	}

	container.subitems.push(newSubIndex);

	return newSubIndex;
}

function addObject(container,predefined) {
	let newObject = document.createElement("div");
	newObject.className = "object-div";
	newObject.subitems = [];

	if(predefined !== undefined) {
		if(predefined.subitems === undefined || predefined.subitems == null)
			predefined.subitems = [{}];

		newObject.object = predefined;
		predefined.subitems.forEach((val,index) => {
			let newSubIndex = addSubIndex(newObject,predefined.index,index);
			newObject.appendChild(newSubIndex);
		});
	} else {
		let object = {};
		object.subitems = [{}];
		newObject.object = object;
		let index = container.address.begin;
		if(container.objects !== undefined && container.objects.length > 0) {
			let lastIndex = parseInt(container.objects[container.objects.length-1].object.index,16);
			index = (lastIndex+0x10);
		}
		let newSubIndex = addSubIndex(newObject,index,0);
		newObject.appendChild(newSubIndex);
		if(currentDevice.objects === undefined) currentDevice.objects = [];

		currentDevice.objects.push(object);
	}

	if(container.objects === undefined) container.objects = [];
	container.objects.push(newObject);

	container.appendChild(newObject);
}

function addGroupOption(groupSelector,newGroup) {
	let newGroupOpt = document.createElement('option');
	newGroupOpt.value = newGroup.type;
	newGroupOpt.label = newGroup.name;
	groupSelector.appendChild(newGroupOpt);

	if(groupSelector.childNodes.length == 1) {
		groupSelector.selectedIndex = 0;
	}
	groupSelector.disabled = false;
}

function addDeviceOption(deviceSelector,newDevice) {
	// First remove any "no options" option
	if(deviceSelector.disabled && deviceSelector.options.length > 0) {
		deviceSelector.remove(0);
	}
	let newDeviceOpt = document.createElement('option');
	newDeviceOpt.value = newDevice.name;
	newDeviceOpt.label = newDevice.name;
	deviceSelector.appendChild(newDeviceOpt);

	if(deviceSelector.childNodes.length == 1) {
		deviceSelector.selectedIndex = 0;
	}
	deviceSelector.disabled = false;
}

function initUI() {
	let esctool = document.getElementById("Esctool");

	let getSpacer = (cls) => {
		let spacer = document.createElement('div');
		spacer.className = cls;
		return spacer;
	};

	if(esctool !== undefined) {
		esctool.className = "esctool-control";

		let saveButton = document.createElement("button");
		saveButton.className = "control-button save-button";
		saveButton.innerHTML = "Save";
		saveButton.onclick = (event) => {
			saveSetup();
			writeConsole("Saved set up");
		};

		let loadButton = document.createElement("button");
		loadButton.className = "control-button load-button";
		loadButton.innerHTML = "Load";
		loadButton.onclick = (event) => {
			loadSetups();
		};

		let exportButton = document.createElement("button");
		exportButton.className = "control-button export-button";
		exportButton.innerHTML = "Export";
		exportButton.onclick = (event) => {
			exportDevice2XML();
		};

		let clearButton = document.createElement("button");
		clearButton.className = "control-button clear-button";
		clearButton.innerHTML = "Clear";
		clearButton.onclick = (event) => {
			if(confirm("Clear setup?")) {
				setup.setups[selectedSetup].groups = [];
				setup.setups[selectedSetup].vendorid = "";
				setup.setups[selectedSetup].vendorname = "";
				saveSetup();
				writeConsole("Cleared set up");
			}
		};

		esctool.appendChild(saveButton);
		esctool.appendChild(loadButton);
		esctool.appendChild(exportButton);
		esctool.appendChild(clearButton);
	}

	let vendor = document.getElementById("Vendor");
	if(vendor !== undefined) {
		vendor.className = "top-div";

		let vendorTopDiv = document.createElement("div");
		vendorTopDiv.className = "top-div-label";
		vendorTopDiv.innerHTML = "Vendor information";

		let vendorIdLabel = document.createElement("div");
		vendorIdLabel.className = "label-div";
		vendorIdLabel.innerHTML = "ID";

		let vendorIdInput = document.createElement("input");
		vendorIdInput.id = vendorIdInputID;
		vendorIdInput.title = "Vendor ID (hex)";
		vendorIdInput.className = "vendorid-input";
		vendorIdInput.innerHTML = "0x00000000";

		vendorIdInput.onchange = (event) => {
			let newValue = parseInt(vendorIdInput.value,16);
			vendorIdInput.value = formatHex(newValue,8);
			setup.setups[selectedSetup].vendorid = vendorIdInput.value;
		};

		let vendorNameLabel = document.createElement("div");
		vendorNameLabel.className = "label-div";
		vendorNameLabel.innerHTML = "Name";

		let vendorNameInput = document.createElement("input");
		vendorNameInput.className = "vendorname-input";
		vendorNameInput.id = vendorNameInputID;
		vendorNameInput.innerHTML = "";
		vendorNameInput.onchange = (event) => {
			setup.setups[selectedSetup].vendorname = vendorNameInput.value;
		};

		vendor.appendChild(vendorTopDiv);
		vendor.appendChild(vendorIdLabel);
		vendor.appendChild(vendorIdInput);
		vendor.appendChild(vendorNameLabel);
		vendor.appendChild(vendorNameInput);
	}

	let groupsDevices = document.getElementById("GroupsDevices");
	if(groupsDevices !== undefined) {
		groupsDevices.className = "top-div";

		let groupsDevicesDiv = document.createElement("div");
		groupsDevicesDiv.className = "top-div-label";
		groupsDevicesDiv.innerHTML = "Groups and devices";

		let groupsLabel = document.createElement("div");
		groupsLabel.className = "label-div";
		groupsLabel.innerHTML = "Groups";

		let groupSelector = document.createElement("select");
		groupSelector.title = "Groups";
		groupSelector.id = groupSelectorID;
		groupSelector.className = "group-selector";
		groupSelector.disabled = true;

		let deleteGroupButton = document.createElement('button');
		deleteGroupButton.className = "group-delete-button";
		deleteGroupButton.title = "Delete group";
		deleteGroupButton.innerHTML = "-";

		deleteGroupButton.onclick = (event) => {
			setup.setups[selectedSetup].groups.every((val,index,arr) => {
				if(val.type == groupSelector.value) {
					if(val.devices !== undefined && val.devices.length > 0) {
						alert("Failed: There are devices in the group");
					} else {
						setup.setups[selectedSetup].groups.splice(index,1);
						groupSelector.remove(val);
						if(groupSelector.options.length == 0) groupSelector.disabled = true;	
					}
					return false;
				}
				return true;
			});
		};

		let groupTypeLabel = document.createElement("div");
		groupTypeLabel.className = "label-div";
		groupTypeLabel.innerHTML = "Type";

		let newGroupTypeInput = document.createElement("input");
		newGroupTypeInput.title = "Type of new group";
		newGroupTypeInput.className = "new-group-type-input";

		let groupNameLabel = document.createElement("div");
		groupNameLabel.className = "label-div";
		groupNameLabel.innerHTML = "Name";

		let newGroupNameInput = document.createElement("input");
		newGroupNameInput.title = "Name of new group";
		newGroupNameInput.className = "new-group-name-input";

		let addGroupButton = document.createElement("button");
		addGroupButton.innerHTML = "Add";
		addGroupButton.title = "Add new group";
		addGroupButton.onclick = (event) => {
			if(newGroupTypeInput.value.length < 3) {
				alert("Group type should be 3 characters or more");
			} else if(newGroupNameInput.value.length < 3) {
				alert("Group name should be 3 characters or more");
			} else {
				setup.setups[selectedSetup].groups.forEach((value,index,arr) => {
					if(value.type == newGroupTypeInput.value) {
						alert("Group type \""+newGroupTypeInput.value+"\" already exists for group \""+value.name+"\"");
						return;
					}
				});

				let newGroup = {};
				newGroup.type = newGroupTypeInput.value; // TODO sanitize
				newGroup.name = newGroupNameInput.value;
				newGroup.devices = [];
				setup.setups[selectedSetup].groups.push(newGroup);

				addGroupOption(groupSelector,newGroup);

				newGroupTypeInput.value = "";
				newGroupNameInput.value = "";
			}
		};

		let devicesLabel = document.createElement("div");
		devicesLabel.className = "label-div";
		devicesLabel.innerHTML = "Devices";

		let deviceSelector = document.createElement("select");
		deviceSelector.title = "Devices";
		deviceSelector.className = "device-selector";
		deviceSelector.disabled = true;

		let deleteDeviceButton = document.createElement('button');
		deleteDeviceButton.className = "device-delete-button";
		deleteDeviceButton.title = "Delete device";
		deleteDeviceButton.innerHTML = "-";

		deleteDeviceButton.onclick = (event) => {
			// Delete device from group
			if(confirm("Are you sure you want to delete device "+deviceSelector.value)) {
				setup.setups[selectedSetup].groups.every((group,index,arr) => {
					if(group.type == groupSelector.value) {
						if(group.devices !== undefined) {
							group.devices.every((dev,index,arr) => {
								if(dev.name == deviceSelector.value) {
									group.devices.splice(index,1);
									return false;
								}
								return true;
							});
						}
						return false;
					}
					return true;
				});
	
				deviceSelector.remove(deviceSelector.selectedIndex);
				if(deviceSelector.options.length == 0) {
					let noDevicesOption = document.createElement("option");
					noDevicesOption.value = "none";
					noDevicesOption.label = "No devices defined";
					deviceSelector.appendChild(noDevicesOption);
					deviceSelector.disabled = true;
				}	
			}
		};

		let deviceNameLabel = document.createElement("div");
		deviceNameLabel.className = "label-div";
		deviceNameLabel.innerHTML = "Name";

		let newDeviceNameInput = document.createElement("input");
		newDeviceNameInput.title = "Name of new device";
		newDeviceNameInput.className = "new-device-name-input";

		let addDeviceButton = document.createElement("button");
		addDeviceButton.innerHTML = "Add";
		addDeviceButton.title = "Add new device";
		addDeviceButton.onclick = (event) => {
			if(newDeviceNameInput.value.length < 3) {
				alert("Device name should be 3 characters or more");
			} else {
				let newDevice = {};
				newDevice.name = newDeviceNameInput.value;
				newDevice.groupType = groupSelector.value;

				initDevice(newDevice);

				setup.setups[selectedSetup].groups.every((val,index,arr) => {
					if(val.type == groupSelector.value) {
						val.devices.push(newDevice);
						return false;
					}
					return true;
				});

				addDeviceOption(deviceSelector,newDevice);
				if(deviceSelector.options.length == 1) {
					deviceSelector.selectedIndex = 0;
					deviceSelector.dispatchEvent(new Event('change')); // Fire change event, to update device section
				}
				newDeviceNameInput.value = "";
			}
		};


		groupsDevices.appendChild(groupsDevicesDiv);

		groupsDevices.appendChild(groupsLabel);
		groupsDevices.appendChild(groupSelector);
		groupsDevices.appendChild(deleteGroupButton);
		groupsDevices.appendChild(getSpacer("spacer-1em"));
		groupsDevices.appendChild(groupTypeLabel);
		groupsDevices.appendChild(newGroupTypeInput);
		groupsDevices.appendChild(groupNameLabel);
		groupsDevices.appendChild(newGroupNameInput);
		groupsDevices.appendChild(addGroupButton);
		groupsDevices.appendChild(getSpacer("spacer-2em"));
		groupsDevices.appendChild(devicesLabel);
		groupsDevices.appendChild(deviceSelector);
		groupsDevices.appendChild(deleteDeviceButton);
		groupsDevices.appendChild(getSpacer("spacer-1em"));
		groupsDevices.appendChild(deviceNameLabel);
		groupsDevices.appendChild(newDeviceNameInput);
		groupsDevices.appendChild(addDeviceButton);

		groupSelector.onchange = (event) => {
			setup.setups[selectedSetup].groups.every((val,index,arr) => {
				if(val.type == groupSelector.value) {
					while(deviceSelector.options.length > 0) {
						deviceSelector.remove(deviceSelector.options.length-1);
					}
					if(val.devices === undefined || val.devices.length == 0) {
						deviceSelector.disabled = true;
						let noDevicesOption = document.createElement("option");
						noDevicesOption.value = "none";
						noDevicesOption.label = "No devices defined";
						deviceSelector.appendChild(noDevicesOption);
					} else {
						val.devices.forEach((dev) => {
							addDeviceOption(deviceSelector,dev);
						});
						deviceSelector.disabled = false;
						deviceSelector.selectedIndex = 0;
						deviceSelector.dispatchEvent(new Event('change')); // Fire change event, to update device section
					}
					return false;
				}
				return true;
			});
		};

		deviceSelector.onchange = (event) => {
			setup.setups[selectedSetup].groups.every((group,index,arr) => {
				if(group.type == groupSelector.value) {
					if(group.devices !== undefined) {
						group.devices.every((dev,index,arr) => {
							if(dev.name == deviceSelector.value) {
								loadDevice(dev);
								return false;
							}
							return true;
						});
					}
					return false;
				}
				return true;
			});
		};
	}

	let deviceView = document.getElementById("DeviceView");
	if(deviceView !== undefined) {
		deviceView.className = "top-div";
		let dvTopLabel = document.createElement("div");
		dvTopLabel.className = "top-div-label";
		dvTopLabel.innerHTML = "Device";

		let dvDeviceIdentityDiv = document.createElement('div');
		dvDeviceIdentityDiv.className = "device-info-div";

		let dvDeviceNameLabel = document.createElement("div");
		dvDeviceNameLabel.className = "label-div";
		dvDeviceNameLabel.innerHTML = "Name";

		let dvDeviceName = document.createElement("input");
		dvDeviceName.id = devViewDeviceNameID;

		let dvDeviceProductCodeLabel = document.createElement("div");
		dvDeviceProductCodeLabel.className = "label-div";
		dvDeviceProductCodeLabel.innerHTML = "Product code";

		let dvDeviceProductCode = document.createElement("input");
		dvDeviceProductCode.id = devViewDeviceProductCodeID;
		dvDeviceProductCode.onchange = (event) => {
			let newValue = parseInt(dvDeviceProductCode.value,16);
			dvDeviceProductCode.value = formatHex(newValue,8);
			currentDevice.productCode = dvDeviceProductCode.value;
		};

		let dvDeviceRevisionNoLabel = document.createElement("div");
		dvDeviceRevisionNoLabel.className = "label-div";
		dvDeviceRevisionNoLabel.innerHTML = "Revision";

		let dvDeviceRevisionNo = document.createElement("input");
		dvDeviceRevisionNo.id = devViewDevicRevisionNoID;
		dvDeviceRevisionNo.onchange = (event) => {
			let newValue = parseInt(dvDeviceRevisionNo.value,16);
			dvDeviceRevisionNo.value = formatHex(newValue,8);
			currentDevice.revisionNo = dvDeviceRevisionNo.value;
		};

		let dvDeviceEepromDiv = document.createElement('div');
		dvDeviceEepromDiv.className = "device-info-div";

		let dvDeviceEepromByteSizeLabel = document.createElement("div");
		dvDeviceEepromByteSizeLabel.className = "label-div";
		dvDeviceEepromByteSizeLabel.innerHTML = "EEPROM Bytesize";

		let dvDeviceEepromByteSize = document.createElement("input");
		dvDeviceEepromByteSize.id = devViewDevicEepromByteSizeID;
		dvDeviceEepromByteSize.title = "Size of EEPROM in bytes";
		dvDeviceEepromByteSize.value = 2048;

		dvDeviceEepromByteSize.onchange = (event) => {
			let newValue = parseInt(dvDeviceEepromByteSize.value,parseRadix(dvDeviceEepromByteSize.value));
			currentDevice.eeprom.ByteSize = newValue;
		};

		let dvDevicePdiControlLabel = document.createElement("div");
		dvDevicePdiControlLabel.className = "label-div";
		dvDevicePdiControlLabel.innerHTML = "PDI Ctrl";

		let dvDevicePdiControl = document.createElement("input");
		dvDevicePdiControl.className = "config-data-input";
		dvDevicePdiControl.id = devViewDevicPdiControlID;
		dvDevicePdiControl.title = "PDI control 0x0140 [7:0] - 0x0141 [15:0]";
		dvDevicePdiControl.value = formatHex(parseInt(eeprom.ConfigData.substring(0,4),16),4);;

		let dvDevicePdiConfigurationLabel = document.createElement("div");
		dvDevicePdiConfigurationLabel.className = "label-div";
		dvDevicePdiConfigurationLabel.innerHTML = "PDI Conf";

		let dvDevicePdiConfiguration = document.createElement("input");
		dvDevicePdiConfiguration.className = "config-data-input";
		dvDevicePdiConfiguration.id = devViewDevicPdiConfigurationID;
		dvDevicePdiConfiguration.title = "PDI configuration 0x0150 [7:0] - 0x0151 [15:0]";
		dvDevicePdiConfiguration.value = formatHex(parseInt(eeprom.ConfigData.substring(4,8),16),4);

		let dvDevicePdiConfiguration2Label = document.createElement("div");
		dvDevicePdiConfiguration2Label.className = "label-div";
		dvDevicePdiConfiguration2Label.innerHTML = "PDI Conf 2";

		let dvDevicePdiConfiguration2 = document.createElement("input");
		dvDevicePdiConfiguration2.className = "config-data-input";
		dvDevicePdiConfiguration2.id = devViewDevicPdiConfiguration2ID;
		dvDevicePdiConfiguration2.title = "PDI configuration 0x0153 [7:0] - 0x0152 [15:0]";
		dvDevicePdiConfiguration2.value = formatHex(parseInt(eeprom.ConfigData.substring(12,16),16),4);

		let dvDeviceSyncImpulseLenLabel = document.createElement("div");
		dvDeviceSyncImpulseLenLabel.className = "label-div";
		dvDeviceSyncImpulseLenLabel.innerHTML = "Sync Impulse Length";

		let dvDeviceSyncImpulseLen = document.createElement("input");
		dvDeviceSyncImpulseLen.className = "config-data-input";
		dvDeviceSyncImpulseLen.id = devViewSyncImpulseLenID;
		dvDeviceSyncImpulseLen.title = "Sync Impulse Length (x 10 ns)";
		dvDeviceSyncImpulseLen.value = formatHex(parseInt(eeprom.ConfigData.substring(8,12),16),4);

		let dvDeviceConfiguredStationAliasLabel = document.createElement("div");
		dvDeviceConfiguredStationAliasLabel.className = "label-div";
		dvDeviceConfiguredStationAliasLabel.innerHTML = "Conf. Station Alias";

		let dvDeviceConfiguredStationAlias = document.createElement("input");
		dvDeviceConfiguredStationAlias.id = devConfiguredStationAliasID;
		dvDeviceConfiguredStationAlias.className = "config-data-input";
		dvDeviceConfiguredStationAlias.title = "Configured Station Alias (0x0012)";
		dvDeviceConfiguredStationAlias.value = formatHex(parseInt(eeprom.ConfigData.substring(16,20),16),4);

		let configDataChange = (event) => {
			if(event !== null) {
				let changedElement = event.target;
				changedElement.value = formatHex(parseInt(changedElement.value,16),4);	
			}

			currentDevice.eeprom.ConfigData =
				formatHexRaw(parseInt(dvDevicePdiControl.value,16),4) +
				formatHexRaw(parseInt(dvDevicePdiConfiguration.value,16),4) +
				formatHexRaw(parseInt(dvDeviceSyncImpulseLenLabel.value,16),4) +
				formatHexRaw(parseInt(dvDevicePdiConfiguration2.value,16),4) +
				formatHexRaw(parseInt(dvDeviceConfiguredStationAlias.value,16),4);
		};

		if(currentDevice !== undefined && currentDevice !== null &&
		   currentDevice.eeprom !== undefined && currentDevice.eeprom.ConfigData == "")
		{
			configDataChange(null);
		}

		dvDevicePdiControl.onchange = configDataChange;
		dvDevicePdiConfiguration.onchange = configDataChange;
		dvDevicePdiConfiguration2.onchange = configDataChange;
		dvDeviceSyncImpulseLen.onchange = configDataChange;
		dvDeviceConfiguredStationAlias.onchange = configDataChange;

		deviceView.appendChild(dvTopLabel);

		dvDeviceIdentityDiv.appendChild(dvDeviceNameLabel);
		dvDeviceIdentityDiv.appendChild(dvDeviceName);
		dvDeviceIdentityDiv.appendChild(getSpacer("spacer-2em"));
		dvDeviceIdentityDiv.appendChild(dvDeviceProductCodeLabel);
		dvDeviceIdentityDiv.appendChild(dvDeviceProductCode);
		dvDeviceIdentityDiv.appendChild(getSpacer("spacer-2em"));
		dvDeviceIdentityDiv.appendChild(dvDeviceRevisionNoLabel);
		dvDeviceIdentityDiv.appendChild(dvDeviceRevisionNo);

		dvDeviceEepromDiv.appendChild(dvDeviceEepromByteSizeLabel);
		dvDeviceEepromDiv.appendChild(dvDeviceEepromByteSize);
		dvDeviceEepromDiv.appendChild(getSpacer("spacer-2em"));
		dvDeviceEepromDiv.appendChild(dvDevicePdiControlLabel);
		dvDeviceEepromDiv.appendChild(dvDevicePdiControl);
		dvDeviceEepromDiv.appendChild(getSpacer("spacer-1em"));
		dvDeviceEepromDiv.appendChild(dvDevicePdiConfigurationLabel);
		dvDeviceEepromDiv.appendChild(dvDevicePdiConfiguration);
		dvDeviceEepromDiv.appendChild(getSpacer("spacer-1em"));
		dvDeviceEepromDiv.appendChild(dvDevicePdiConfiguration2Label);
		dvDeviceEepromDiv.appendChild(dvDevicePdiConfiguration2);
		dvDeviceEepromDiv.appendChild(getSpacer("spacer-1em"));
		dvDeviceEepromDiv.appendChild(dvDeviceSyncImpulseLenLabel);
		dvDeviceEepromDiv.appendChild(dvDeviceSyncImpulseLen);
		dvDeviceEepromDiv.appendChild(getSpacer("spacer-1em"));
		dvDeviceEepromDiv.appendChild(dvDeviceConfiguredStationAliasLabel);
		dvDeviceEepromDiv.appendChild(dvDeviceConfiguredStationAlias);

		deviceView.appendChild(dvDeviceIdentityDiv);
		deviceView.appendChild(dvDeviceEepromDiv);
	}

	let addrLayout = document.getElementById("AddressLayout");
	if(addrLayout !== undefined) {
		addrs.forEach((addr) => {
			let addrDiv = document.createElement("div");
			addrDiv.className = "top-div";
			addrDiv.address = addr;
			addrDiv.id = "addr" + formatHexRaw(addr.begin,4);

			let addrTopLabel = document.createElement("div");
			addrTopLabel.className = "top-div-label";
			addrTopLabel.innerHTML = "Address" + " " +
				formatHex(addr.begin,4)+"-"+formatHex((addr.begin+addr.length),4) + " " +
				"("+ addr.label +")";

			let addButton = document.createElement("button");
			addButton.className = "object-add-button";
			addButton.id = "add-addr-" + addr.begin;
			addButton.title = "Add object";
			addButton.innerHTML = "+";
			addButton.onclick = (event) => {
				addObject(addrDiv);
			};

			addrTopLabel.appendChild(addButton);

			addrDiv.appendChild(addrTopLabel);

			addrLayout.appendChild(addrDiv);
		});
	}

	let consoleDiv = document.getElementById("Console");
	if(consoleDiv !== undefined) {
		consoleDiv.className = "esctool-console";
	}

	loadSetups();
}

var mandatoryObjects = [];

function createSubItem(subitemIdx,datatype,name,defaultData,access) {
	let obj = {
		subIdx: subitemIdx,
		datatype: datatype,
		name: name,
		defaultData: defaultData,
		access: access
	};
	return obj;
}

function createObject(index,type,subitems) {
	let obj = {
		index: index,
		type: type,
		subitems: subitems
	}
	return obj;
}

function fillMandatoryObjects() {
	// ETG1000.6 section 5.6.7
	mandatoryObjects = []; // clear if exists

	mandatoryObjects.push(createObject("0x1000",variableStr,[createSubItem(0x0,"UDINT","Device Type","5001",readOnlyStr)]));
	mandatoryObjects.push(createObject("0x1001",variableStr,[createSubItem(0x0,"USINT","Error register","00",readOnlyStr)]));
	mandatoryObjects.push(createObject("0x1008",variableStr,[createSubItem(0x0,"VISIBLESTRING","Device name",currentDevice.name,readOnlyStr)]));
	mandatoryObjects.push(createObject("0x100A",variableStr,[createSubItem(0x0,"VISIBLESTRING","Manufacturer Hardware version","N/A",readOnlyStr)]));

	// 5.6.7.4.6
	mandatoryObjects.push(createObject("0x1018",recordStr,[
		createSubItem(0x00,"USINT","Identity Object","0x04",readOnlyStr),
		createSubItem(0x01,"UDINT","Vendor ID",setup.setups[selectedSetup].vendorid,readOnlyStr),
		createSubItem(0x02,"UDINT","Product Code",currentDevice.productCode,readOnlyStr),
		createSubItem(0x03,"UDINT","Revision Number",currentDevice.revisionNo,readOnlyStr),
		createSubItem(0x04,"UDINT","Serial Number","0x0",readOnlyStr),
		]));

	// PDO mapping 5.6.7.4.7 / 5.6.7.4.8
	var pdoObjects = {
		rx: [],
		tx: []
	};
	pdoTable.forEach((pdoDesc) => {
		pdoDesc.bitLength = 0;
		let pdoIndex = 0;
		currentDevice.objects.forEach((obj) => {
			let pdoMappingObj = {
				type: recordStr,
				index: formatHex(pdoDesc.index+pdoIndex,4),
				subitems: []
			};

			let subitem0 = {
				subIdx: "0x0",
				datatype: "USINT",
				name: ((obj.subitems[0].name !== undefined && obj.subitems[0].name !== "") ?
					obj.subitems[0].name : "unnamed")+" "+"process data mapping"
			}

			pdoMappingObj.subitems.push(subitem0);
			let mappings = 0;
			obj.subitems.forEach((subitem,index) => {
				if(index == 0) return;
				if(subitem.pdo !== undefined &&
					subitem.pdo == pdoDesc.type)
				{
					let bitSize = getBitSize(subitem.datatype);
					pdoDesc.bitLength += bitSize;
					let pdoMappingSubItem = {
						subIdx: mappings + 1,
						datatype: "UDINT",
						name: "SubIndex " + formatDec(mappings+1,3),
						defaultData: formatHexRaw(parseInt(obj.index,16),4)+
							formatHexRaw(subitem.subIdx,2)+formatHexRaw(bitSize,2)
					};
					pdoMappingObj.subitems.push(pdoMappingSubItem);
					++mappings;
				}
			});
			if(mappings > 0) {
				subitem0.defaultdata = mappings.toString();
				subitem0.bitsize = mappings*getBitSize("UDINT") + 16; // +16 for subindex 0
				mandatoryObjects.push(pdoMappingObj);
				pdoObjects[pdoDesc.type].push(pdoMappingObj);
				++pdoIndex;
			}
		});
		if(pdoDesc.type == pdoRXVal) {
			currentDevice.syncManagerSetup.conf[currentDevice.syncManagerSetup.rxpdo].DefaultSize = (pdoDesc.bitLength/8) + (pdoDesc.bitLength % 8);
		} else {
			currentDevice.syncManagerSetup.conf[currentDevice.syncManagerSetup.txpdo].DefaultSize = (pdoDesc.bitLength/8) + (pdoDesc.bitLength % 8);
		}
	});

	// 5.6.7.4.9
	mandatoryObjects.push(createObject("0x1C00",arrayStr,[
		createSubItem(0x00,"USINT","Sync manager type","0x04",readOnlyStr),
		createSubItem(0x01,null,null,currentDevice.syncManagerSetup.type[0].toString()),
		createSubItem(0x02,null,null,currentDevice.syncManagerSetup.type[1].toString()),
		createSubItem(0x03,null,null,currentDevice.syncManagerSetup.type[2].toString()),
		createSubItem(0x04,null,null,currentDevice.syncManagerSetup.type[3].toString()),
		]));

	// 5.6.7.4.10.1
	if(pdoObjects[pdoRXVal].length > 0) {
		console.log(pdoObjects[pdoRXVal].length+" RXPDO's");
		let smObjDict = {
			index: formatHex(0x1C12,4),
			type: arrayStr,
			subitems: [
				{
				subIdx: "0x00",
				name: "SyncManager 2 assignment", // TODO in case SM2 isnt RXPDO
				datatype: "UINT", // Datatype of each array element
				defaultData: pdoObjects[pdoRXVal].length.toString()
				}
			]
		};
		let subIdx = 1;
		pdoObjects[pdoRXVal].forEach((rxpdo) => {
			smObjDict.subitems.push(
				createSubItem(subIdx++,null,null,formatHexRaw(parseInt(rxpdo.index,16),4))
			);
		});
		mandatoryObjects.push(smObjDict);
	}

	if(pdoObjects[pdoTXVal].length > 0) {
		console.log(pdoObjects[pdoTXVal].length+" TXPDO's");
		let smObjDict = {
			index: formatHex(0x1C13,4),
			type: arrayStr,
			subitems: [
				{
				subIdx: "0x00",
				name: "SyncManager 3 assignment", // TODO in case SM3 isnt RXPDO
				datatype: "UINT", // Datatype of each array element
				defaultData: pdoObjects[pdoTXVal].length.toString()
				}
			]
		};
		let subIdx = 1;
		pdoObjects[pdoTXVal].forEach((txpdo) => {
			smObjDict.subitems.push(
				createSubItem(subIdx++,null,null,txpdo.index.toString())
			);
		});
		mandatoryObjects.push(smObjDict);
	}
	// 1C32, 1C33
}

function hex2XML(hexStr) {
	let val = parseInt(hexStr,16);
	if(isNaN(val)) val = 0;
	return "#x"+(val.toString(16).toUpperCase());
}

function getBitSize(datatype) {
	if(datatype == "USINT" || datatype == "SINT") {
		return 8;
	} else
	if(datatype == "UINT" || datatype == "INT") {
		return 16;
	} else
	if(datatype == "UDINT" || datatype == "DINT") {
		return 32;
	} else
	if(datatype == "UINT64" || datatype == "INT64") {
		return 64;
	}
	return 0;
	// TODO Check for potential STRING datatype bitsize?
}

// TODO: For now, no octal numbers :)
function parseRadix(numberStr) {
	if(numberStr.indexOf("x") != -1) {
		return 16;
	}
	return 10;
}

function createDataTypes(datatypes) {
	// Create generic types, TODO only create used ones
	dataTypes.push({
		Name: "USINT",
		BitSize: getBitSize("USINT")
	},
	{
		Name: "SINT",
		BitSize: getBitSize("SINT")
	},
	{
		Name: "INT",
		BitSize: getBitSize("INT")
	},
	{
		Name: "UINT",
		BitSize: getBitSize("UINT")
	},
	{
		Name: "UDINT",
		BitSize: getBitSize("UDINT")
	},
	{
		Name: "DINT",
		BitSize: getBitSize("DINT")
	},
	{
		Name: "UINT64",
		BitSize: getBitSize("UINT64")
	},
	{
		Name: "INT64",
		BitSize: getBitSize("INT64")
	}
	);

	let checkStringDT = (str) => {
		let dtName = "STRING("+str.length+")";
		let exists = false;
		datatypes.every((datatype) => {
			exists = (datatype.Name == dtName);
			return !exists;
		});
		return exists;
	};

	let createStringDT = (str) => {
		console.log("Creating string datatype for '"+str+"'");
		let stringDT = {
			Name: "STRING("+str.length+")",
			BitSize: str.length*8+((str.length*8)%16) // TODO: Is 16-bit alignment correct?

		};
		datatypes.push(stringDT);
	};

	let collection = [mandatoryObjects,currentDevice.objects];
	collection.forEach((objects) => {
		objects.forEach((obj) => {
			let datatype = null;
			if(obj.type == recordStr || obj.type === undefined) {
				datatype = {
					Name: "DT"+formatHexRaw(parseInt(obj.index,16),4),
					BitSize: 0,
					subitems: []
				}
				let subIdx = 0;
				obj.subitems.forEach((subitem,index) => {
					let dt = subitem.datatype !== undefined ?
						subitem.datatype.toUpperCase() : null;
					if(dt == null) {
						console.log(obj.index+" "+subitem.name+" has no datatype, skipping...");
						return;
					}
					if((dt.indexOf("STRING") != -1) &&
						subitem.defaultData !== undefined &&
						subitem.defaultData != null &&
						subitem.defaultData != "")
					{
						if(!checkStringDT(subitem.defaultData))
							createStringDT(subitem.defaultData);
					} else if(datatype !== null) {
						let bitsize = getBitSize(dt);
						if(datatype.subitems === undefined) datatype.subitems = [];
						let itemSubIdx = subitem.subidx;
						if(itemSubIdx === undefined) itemSubIdx = subIdx;
						let access = subitem.access;
						if(subitem.access === undefined) access = readOnlyStr;
						datatype.subitems.push({
							SubIdx: itemSubIdx,
							Name: itemSubIdx == 0 ? "SubIndex 000" : subitem.name,
							Type: dt,
							BitSize: bitsize,
							BitOffs: datatype.BitSize,
							Access: access
						});
						datatype.BitSize += bitsize;
						// 16-bit alignment
						if(itemSubIdx == 0) datatype.BitSize += (datatype.BitSize%16);
					}
					++subIdx;
				});
				// 16-bit alignment
				datatype.BitSize += (datatype.BitSize%16);
				datatypes.push(datatype);
			} else if(obj.type == variableStr) {
				let subitem = obj.subitems[0];

				let dt = subitem.datatype !== undefined ?
					subitem.datatype.toUpperCase() : null;
				if(dt == null) {
					console.log(obj.index+" "+subitem.name+" has no datatype, skipping...");
					return;
				}
				if((dt.indexOf("STRING") != -1) &&
					subitem.defaultData !== undefined &&
					subitem.defaultData != null &&
					subitem.defaultData != "")
				{
					if(!checkStringDT(subitem.defaultData))
						createStringDT(subitem.defaultData);
				}
			} else if(obj.type == arrayStr) {
				let arrayInfo = obj.subitems[0];
				console.log(arrayInfo);
				let dt = arrayInfo.datatype !== undefined ?
					arrayInfo.datatype.toUpperCase() : null;
				if(dt == null) {
					console.log(obj.index+" has no datatype, skipping...");
				}
				let bitsize = getBitSize(dt);
				let elemCount = parseInt(arrayInfo.defaultData,parseRadix(arrayInfo.defaultData));
				datatype = {
					Name: "DT"+formatHexRaw(parseInt(obj.index,16),4),
					// Size of BaseType * item count + 16 bit aligned index 0
					BitSize: bitsize*elemCount+16,
					BaseType: dt,
					subitems: [
						{
							SubIdx: 0,
							Name: "SubIndex 000",
							Type: "USINT",
							BitSize: 8,
							BitOffs: 0,
							Access: obj.subitems[0].access
						},
						{
							// SubIdx remains undefined
							Name: "Elements",
							Type: "DT"+formatHexRaw(parseInt(obj.index,16),4)+"ARR",
							BitSize: bitsize*elemCount,
							BitOffs: 16
						}
					]
				}
				datatypes.push(datatype);
				datatype = {
					Name: "DT"+formatHexRaw(parseInt(obj.index,16),4)+"ARR",
					BitSize: bitsize*elemCount,
					BaseType: dt,
					ArrayInfo: {
						LBound: 1,
						Elements: elemCount
					}
				}
				datatypes.push(datatype);
			}
		});
	});

	if(!checkStringDT(setup.setups[selectedSetup].vendorname))
		createStringDT(setup.setups[selectedSetup].vendorname);

//	printDataTypes(datatypes);
}

function printDataTypes(datatypes) {
	datatypes.forEach((datatype) => {
		console.log("Name: "+datatype.Name);
		console.log("BitSize: "+datatype.BitSize);
		if(datatype.subitems !== undefined) {
			datatype.subitems.forEach((subitem) => {
				console.log("SubItem");
				console.log("\tSubIdx: "+subitem.SubIdx);
				console.log("\tName: "+subitem.Name);
				console.log("\tType: "+subitem.Type);
				console.log("\tBitSize: "+subitem.BitSize);
				console.log("\tBitOffs: "+subitem.BitOffs);
				console.log("\tAccess: "+subitem.Access);
			});
		}
		if(datatype.BaseType !== undefined) {
			console.log("BaseType: "+datatype.BaseType);
		}
		if(datatype.ArrayInfo !== undefined) {
			console.log("ArrayInfo: ");
			console.log("\tElements: "+datatype.ArrayInfo.Elements);
			console.log("\tBound: "+datatype.ArrayInfo.LBound);
		}
		console.log("\n");
	});
}

function str2asciiStr(str) {
	let asciiStr = "";
	for(let idx = 0; idx < str.length; ++idx) {
		asciiStr += formatHexRaw((str.charCodeAt(idx) & 0xFF),2);
	}
	return asciiStr;
}

function exportDevice2XML() {
	let deviceGroup = null;
	setup.setups[selectedSetup].groups.every((group) => {
		let groupSelector = document.getElementById(groupSelectorID);
		if(groupSelector !== undefined && groupSelector != null) {
			let selectedGroup = groupSelector.selectedOptions.item(groupSelector.selectedIndex);
			if(group.type == selectedGroup.value) {
				deviceGroup = group;
				return false;
			}
		}
		return true;
	});

	if(deviceGroup === undefined || deviceGroup == null) {
		deviceGroup = { value: "(null)", label: "(null)"}
	}

	fillMandatoryObjects();

	// Ensure objects are sequentially ordered by index
	currentDevice.objects.sort((a,b) => {
		if(parseInt(a.index,16) < parseInt(b.index,16)) {
			return -1;
		} else if (parseInt(a.index,16) > parseInt(b.index,16)) {
			return 1;
		}
		return 0;
	});

	// Clear datatypes
	dataTypes = [];
	createDataTypes(dataTypes);

	// Build XML document
	let XML = new XMLWriter();
	XML.writeStartDocument();
	XML.writeStartElement("EtherCATInfo");
	XML.writeAttributeString("xmlns:xsd","http://www.w3.org/2001/XMLSchema");
	XML.writeAttributeString("xmlns:xsi","http://www.w3.org/2001/XMLSchema-instance");
	XML.writeAttributeString("xsi:noNamespaceSchemaLocation","EtherCATInfo.xsd");
	XML.writeAttributeString("Version","1.6");

	XML.writeStartElement("Vendor");
		XML.writeElementString("Id",hex2XML(setup.setups[selectedSetup].vendorid));
		XML.writeStartElement("Name");
		XML.writeCDATA(setup.setups[selectedSetup].vendorname);
		XML.writeEndElement();
	XML.writeEndElement();

	XML.writeStartElement("Descriptions");
		XML.writeStartElement("Groups");
			XML.writeStartElement("Group");
				XML.writeElementString("Type",deviceGroup.type);
				XML.writeElementString("Name",deviceGroup.name);
			XML.writeEndElement();
		XML.writeEndElement();
		XML.writeStartElement("Devices");
			XML.writeStartElement("Device");
			XML.writeAttributeString("Physics","YY"); // TODO not hardcode
				XML.writeStartElement("Type")
				XML.writeAttributeString("ProductCode",hex2XML(currentDevice.productCode));
				XML.writeAttributeString("RevisionNo",hex2XML(currentDevice.revisionNo));
				XML.writeXML(currentDevice.name);
				XML.writeEndElement(); // Type

				XML.writeElementString("Name",currentDevice.name);
				// TODO: Info section
				XML.writeElementString("GroupType",deviceGroup.type);

				XML.writeStartElement("Profile");
					XML.writeElementString("ProfileNo",currentDevice.profileNo);

					XML.writeStartElement("Dictionary");
						XML.writeStartElement("DataTypes");
						dataTypes.forEach((datatype) => {
							XML.writeStartElement("DataType");
							XML.writeElementString("Name",datatype.Name);
							XML.writeElementString("BitSize",datatype.BitSize.toString());
							if(datatype.subitems !== undefined) {
								datatype.subitems.forEach((subitem) => {
									XML.writeStartElement("SubItem");
									if(subitem.SubIdx !== undefined) {
										XML.writeElementString("SubIdx",subitem.SubIdx.toString());
									}
									XML.writeElementString("Name",subitem.Name);
									XML.writeElementString("Type",subitem.Type);
									XML.writeElementString("BitSize",subitem.BitSize.toString());
									XML.writeElementString("BitOffs",subitem.BitOffs.toString());
									if(subitem.Access !== undefined) {
										XML.writeStartElement("Flags");
											XML.writeElementString("Access",subitem.Access);
										XML.writeEndElement(); // Flags
									}
									XML.writeEndElement(); // SubItem
								});
							}
							if(datatype.BaseType !== undefined) {
								XML.writeElementString("BaseType",datatype.BaseType);
							}
							if(datatype.ArrayInfo !== undefined) {
								XML.writeStartElement("ArrayInfo");
								XML.writeElementString("Elements",datatype.ArrayInfo.Elements.toString());
								XML.writeElementString("LBound",datatype.ArrayInfo.LBound.toString());
								XML.writeEndElement(); // ArrayInfo
							}
							XML.writeEndElement(); // DataType
						});
						XML.writeEndElement(); // DataTypes

						XML.writeStartElement("Objects");
						let collection = [mandatoryObjects,currentDevice.objects];
						collection.forEach((objects) => {
							objects.forEach((obj) => {
								XML.writeStartElement("Object");
									XML.writeElementString("Index",hex2XML(obj.index));
									XML.writeElementString("Name",obj.subitems[0].name);
									if(variableStr == obj.type) {
										let dt = obj.subitems[0].datatype.toUpperCase();
										if(dt.indexOf("STRING") != -1) {
											let datalength = 0;
											if(obj.subitems[0].defaultData !== undefined) {
												datalength = obj.subitems[0].defaultData.length;
											}
											let stringDTName = "STRING("+datalength+")";
											XML.writeElementString("Type",stringDTName);
											dataTypes.every((datatype) => {
												if(datatype.Name == stringDTName) {
													XML.writeElementString("BitSize",datatype.BitSize.toString());
													return false;
												}
												return true;
											});
											if(datalength > 0) {
												XML.writeStartElement("Info");
													XML.writeElementString("DefaultData",str2asciiStr(obj.subitems[0].defaultData));
												XML.writeEndElement(); // Info
											}
										} else {
											XML.writeElementString("Type",obj.subitems[0].datatype);
											dataTypes.every((datatype) => {
												if(datatype.Name == dt) {
													XML.writeElementString("BitSize",datatype.BitSize.toString());
													return false;
												}
												return true;
											});
										}
										if(obj.subitems[0].access !== undefined) {
											XML.writeStartElement("Flags");
												XML.writeElementString("Access",obj.subitems[0].access);
											XML.writeEndElement() // Flags;
										}
									} else if(recordStr == obj.type || arrayStr == obj.type) {
										let name = obj.subitems[0].name;
										if(name === undefined || name == null || name == "") name = "Unnamed";
										let dtName = "DT"+formatHexRaw(obj.index,4);
										let datatype = null;
										XML.writeElementString("Type",dtName);
										dataTypes.every((dt) => {
											if(dt.Name == dtName) {
												datatype = dt;
												XML.writeElementString("BitSize",dt.BitSize.toString());
												return false;
											}
											return true;
										});
										XML.writeStartElement("Info");
										obj.subitems.forEach((subitem,index) => {
											XML.writeStartElement("SubItem");
											if(index == 0) {
												XML.writeElementString("Name","SubIndex 000");
												let length = formatHexRaw(obj.subitems.length-1,2);
												XML.writeStartElement("Info");
												XML.writeElementString("DefaultData",length);
												XML.writeEndElement(); // Info
											} else {
												if(recordStr == obj.type) {
													if(subitem.name !== undefined && subitem.name != "")
														XML.writeElementString("Name", subitem.name);
													XML.writeStartElement("Info");
													if(subitem.defaultData !== undefined &&
													subitem.defaultData != null &&
													subitem.defaultData != "")
													{
														// TODO endianess?
														XML.writeElementString("DefaultData",subitem.defaultData);
													} else {
														let zeros = (parseInt(datatype.subitems[index].BitSize)/8)*2;
														XML.writeElementString("DefaultData",formatHexRaw(0,zeros));
													}
													XML.writeEndElement(); // Info
												} else {
													let name = "SubIndex " + formatDec(index,3);
													XML.writeElementString("Name",name);
													XML.writeStartElement("Info");
													if(subitem.defaultData !== undefined &&
													subitem.defaultData !== null &&
													subitem.defaultData != "")
													{
														// TODO endianess? formatHexRaw?
														XML.writeElementString("DefaultData",subitem.defaultData);
													} else {
														let zeros = (parseInt(getBitSize(datatype.BaseType))/8)*2;
														XML.writeElementString("DefaultData",formatHexRaw(0,zeros));
													}
													XML.writeEndElement(); // Info
												}
											}
											XML.writeEndElement(); // SubItem
										});
										XML.writeEndElement() // Info;
									}
								XML.writeEndElement(); // Object
							});
						});
						XML.writeEndElement(); // Objects
					XML.writeEndElement(); // Dictionary
				XML.writeEndElement(); // Profile

				// TODO: Should we decide this somewhere?
				XML.writeElementString("Fmmu","Outputs");
				XML.writeElementString("Fmmu","Inputs");
				XML.writeElementString("Fmmu","MBoxState");

				currentDevice.syncManagerSetup.conf.forEach((sm) => {
					XML.writeStartElement("Sm");
					if(sm.MinSize !== undefined) XML.writeAttributeString("MinSize",hex2XML(sm.MinSize.toString(16)));
					if(sm.MaxSize !== undefined) XML.writeAttributeString("MaxSize",hex2XML(sm.MaxSize.toString(16)));
					XML.writeAttributeString("DefaultSize",hex2XML(sm.DefaultSize.toString(16)));
					XML.writeAttributeString("StartAddress",hex2XML(sm.StartAddress.toString(16)));
					XML.writeAttributeString("ControlByte",hex2XML(sm.ControlByte.toString(16)));
					XML.writeAttributeString("Enable",sm.Enable);
					XML.writeXML(sm.Description);
					XML.writeEndElement(); // Sm	
				});

				// PDO section begin
				let rxPdoLength = 0;
				let txPdoLength = 0;
				pdoTable.forEach((pdoDesc) => {
					mandatoryObjects.forEach((obj) => {
						let index = parseInt(obj.index,16);
						if(index >= pdoDesc.index && index < (pdoDesc.index+pdoDesc.length)) {
							if(pdoDesc.type == pdoRXVal) {
								XML.writeStartElement("RxPdo");
							} else {
								XML.writeStartElement("TxPdo");
							}
							XML.writeAttributeString("Mandatory","true");
							XML.writeAttributeString("Fixed","true");
							if(pdoDesc.type == pdoRXVal) {
								XML.writeAttributeString("Sm",currentDevice.syncManagerSetup.rxpdo.toString());
							} else {
								XML.writeAttributeString("Sm",currentDevice.syncManagerSetup.txpdo.toString());
							}
							XML.writeElementString("Index",hex2XML(obj.index));
							XML.writeElementString("Name",obj.subitems[0].name);
							obj.subitems.forEach((pdoMapping) => {
								if(pdoMapping.subIdx == 0) return;
								XML.writeStartElement("Entry");
								// TODO: Endiannes
								let mappedIndex = pdoMapping.defaultData.substring(0,4);
								let mappedSubIndex = parseInt(pdoMapping.defaultData.substring(4,6),16);
								let mappedBitLen = parseInt(pdoMapping.defaultData.substring(6),16);
								if(pdoDesc.type == pdoRXVal) {
									rxPdoLength += mappedBitLen;
								} else {
									txPdoLength += mappedBitLen;
								}
								XML.writeElementString("Index",hex2XML(mappedIndex));
								XML.writeElementString("SubIndex",mappedSubIndex.toString());
								XML.writeElementString("BitLen",mappedBitLen.toString());
								currentDevice.objects.forEach((mappedObj) => {
									if(parseInt(mappedObj.index,16) == parseInt(mappedIndex,16)) {
										if(mappedSubIndex >= mappedObj.subitems.length) {
											console.log("Mapped PDO "+mappedIndex+"."+mappedSubIndex+" in "+obj.index+"."+pdoMapping.subIdx+" is larger than subitem count");
											return;
										}
										XML.writeElementString("Name",mappedObj.subitems[mappedSubIndex].name);
										XML.writeElementString("DataType",mappedObj.subitems[mappedSubIndex].datatype);
									}
								});
								XML.writeEndElement(); // Entry	
							});
							XML.writeEndElement(); // TxPdo/RxPdo	
						}
					});
/*					if(pdoDesc.type == pdoRXVal) {
						console.log("RxPdo bitlength: "+rxPdoLength);
						if(rxPdoLength != pdoDesc.bitLength) {
							console.log("RxPdo bitlength doesn't match: "+rxPdoLength+" vs "+pdoDesc.bitLength);
						} else {
							console.log("RxPdo bitlength matches");
						}
					} else {
						console.log("TxPdo bitlength: "+txPdoLength);
						if(txPdoLength != pdoDesc.bitLength) {
							console.log("TxPdo bitlength doesn't match: "+txPdoLength+" vs "+pdoDesc.bitLength);
						} else {
							console.log("TxPdo bitlength matches");
						}
					}
*/				});
				// PDO section end

				XML.writeStartElement("Mailbox");
				XML.writeAttributeString("DataLinkLayer",currentDevice.mailboxSetup.DataLinkLayer.toString());
				if(currentDevice.mailboxSetup.CoE !== undefined) {
					XML.writeStartElement("CoE");
					XML.writeAttributeString("SdoInfo",currentDevice.mailboxSetup.CoE.SdoInfo.toString());
					XML.writeAttributeString("PdoAssign",currentDevice.mailboxSetup.CoE.PdoAssign.toString());
					XML.writeAttributeString("PdoConfig",currentDevice.mailboxSetup.CoE.PdoConfig.toString());
					XML.writeAttributeString("CompleteAccess",currentDevice.mailboxSetup.CoE.CompleteAccess.toString());
					XML.writeAttributeString("SegmentedSdo",currentDevice.mailboxSetup.CoE.SegmentedSdo.toString());
					XML.writeEndElement(); // CoE
				}
				XML.writeEndElement(); // Mailbox

				XML.writeStartElement("Dc");
				currentDevice.dcSetup.opModes.forEach((opmode) => {
					XML.writeStartElement("OpMode");
					XML.writeElementString("Name",opmode.Name);
					XML.writeElementString("Desc",opmode.Desc);
					XML.writeElementString("AssignActivate",hex2XML(opmode.AssignActivate.toString(16)));
					if(opmode.CycleTymeSync0 !== undefined) {
						XML.writeStartElement("CycleTymeSync0");
						XML.writeAttributeString("Factor",opmode.CycleTymeSync0.Factor);
						XML.writeXML(opmode.CycleTymeSync0.value.toString());
						XML.writeEndElement(); //CycleTymeSync0
					}
					if(opmode.CycleTymeSync1 !== undefined) {
						XML.writeStartElement("CycleTymeSync1");
						XML.writeAttributeString("Factor",opmode.CycleTymeSync1.Factor);
						XML.writeXML(opmode.CycleTymeSync1.value.toString());
						XML.writeEndElement(); //CycleTymeSync1
					}
					XML.writeEndElement(); // Dc
				});
				XML.writeEndElement(); // Dc

				XML.writeStartElement("Eeprom");
				XML.writeElementString("ByteSize",currentDevice.eeprom.ByteSize.toString());
				// TODO endianness ?
				XML.writeElementString("ConfigData",currentDevice.eeprom.ConfigData);
				XML.writeEndElement(); // Eeprom
			XML.writeEndElement(); // Device
		XML.writeEndElement(); // Devices
	XML.writeEndElement();

	XML.writeEndElement();
	XML.writeEndDocument();

	let xmlDocument = XML.flush();
//	console.log(xmlDocument);
	console.log("Document size: "+xmlDocument.length+" bytes");
	let xhr = new XMLHttpRequest();
// 
	xhr.open("POST", "/export/"+currentDevice.name, true);
	xhr.setRequestHeader("Content-Type", "text/xml");
	xhr.onreadystatechange = () => {
		if (xhr.readyState === XMLHttpRequest.DONE && xhr.status === 200) {
			console.log("exported");
		}
		// TODO failure handling
	};
	
	xhr.send(xmlDocument);
}
