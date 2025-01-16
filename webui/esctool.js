var localStorageName = "esctool";

const vendorIdInputID = "vendor-id-input";
const vendorNameInputID = "vendor-name-input";
const groupSelectorID = "group-selector";
const devViewDeviceNameID = "dv-device-name";
const devViewDeviceProductCodeID = "dv-product-code";
const devViewDevicRevisionNoID = "dv-revision-no";

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

var addrs = [
	{ begin: 0x2000, length: 0x3fff, label: "Manufacturer specific area" },
	{ begin: 0x6000, length: 0xfff, label: "Input area" },
	{ begin: 0x7000, length: 0xfff, label: "Output area" }
];

function formatHex(numberDec,leadingZeros) {
	if(isNaN(numberDec)) numberDec = 0;
	return "0x"+("00000000"+numberDec.toString(16).toUpperCase()).slice(-leadingZeros);
}

function formatHexRaw(numberDec,leadingZeros) {
	if(isNaN(numberDec)) numberDec = 0;
	return ("00000000"+numberDec.toString(16).toUpperCase()).slice(-leadingZeros);
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
	let configurationsObjStr = localStorage.getItem(localStorageName);
	if(configurationsObjStr !== undefined && configurationsObjStr != null) {
		setup = JSON.parse(configurationsObjStr);
		loadSetup(setup.setups[selectedSetup]);
	} else writeConsole("No set up found.");
}

function saveSetup() {
	localStorage.setItem(localStorageName,JSON.stringify(setup));
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
	currentDevice = device;

	currentDevice.profileNo = "5001"; // TODO: not hardcode
}

function selectIndexWithValue(selector,value) {
	for(let i = 0; i < selector.options.length; ++i) {
		if(selector.options.item(i).value == value) {
			selector.selectedIndex = i;
			break;
		}
	}
}

function addSubIndex(container,index,subIndexNo) {
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
		["Record","RECORD"],
		["Variable","VARIABLE"],
		["Array","ARRAY"]
	];

	if(subIndexNo > 0) {
		typeSelector.disabled = true;
	} else {
		populateSelector(typeSelector,typeOptions);
		typeSelector.onchange = (event) => {
			container.object.type = typeSelector.value;
		}
		if(container.object.type !== undefined) {
			selectIndexWithValue(typeSelector,container.object.type);
		}
	}

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

	if(container.object.subitems !== undefined &&
	   container.object.subitems[subIndexNo] !== undefined &&
	   container.object.subitems[subIndexNo].datatype !== undefined)
	{
		subIndexDataTypeInputField.value = container.object.subitems[subIndexNo].datatype;
	} else if(subIndexNo == 0) {
		subIndexDataTypeInputField.value = "USINT";
		subIndexDataTypeInputField.dispatchEvent(new Event('change')); // Fire change event, to update subitem
	}

	subIndexDataTypeInputField.onchange = (event) => {
		container.object.subitems[subIndexNo].datatype = subIndexDataTypeInputField.value;
	};

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

	let objectDefaultValueInputField = document.createElement("input");
	objectDefaultValueInputField.title = "Default value";
	objectDefaultValueInputField.className = "object-default-value-input";

	if(container.object.subitems !== undefined &&
		container.object.subitems[subIndexNo] !== undefined &&
		container.object.subitems[subIndexNo].defaultValue !== undefined)
	{
		objectDefaultValueInputField.value = container.object.subitems[subIndexNo].defaultValue;
	}

	objectDefaultValueInputField.onchange = (event) => {
		container.object.subitems[subIndexNo].defaultValue = objectDefaultValueInputField.value;
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
		if(accessTypeSelector.selectedOptions[0].value.indexOf("R") != -1) {
			readRestrictionsSelector.disabled = false;
		} else {
			readRestrictionsSelector.selectedIndex = 0;
			readRestrictionsSelector.disabled = true;
		}
		if(accessTypeSelector.selectedOptions[0].value.indexOf("W") != -1) {
			writeRestrictionsSelector.disabled = false;
		} else {
			writeRestrictionsSelector.selectedIndex = 0;
			writeRestrictionsSelector.disabled = true;
		}
		container.object.subitems[subIndexNo].access = accessTypeSelector.value;
	};

	let accessOpts = [
		["Read Only","RO"],
		["Read/Write","RW"],
		["Write only","WO"]
	];

	populateSelector(accessTypeSelector,accessOpts);

	if(container.object.subitems !== undefined &&
	   container.object.subitems[subIndexNo] !== undefined &&
	   container.object.subitems[subIndexNo].access !== undefined)
	{
		selectIndexWithValue(accessTypeSelector,container.object.subitems[subIndexNo].access);
	}

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
		["RX","rx"],
		["TX","tx"]
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
	newSubIndex.appendChild(objectDefaultValueInputField);
	newSubIndex.appendChild(accessTypeSelector);
	newSubIndex.appendChild(readRestrictionsSelector);
	newSubIndex.appendChild(writeRestrictionsSelector);
	newSubIndex.appendChild(pdoSelector);

	newSubIndex.addrInputField = addrInputField;
	newSubIndex.subIndexNo = subIndexNo;
	newSubIndex.subIndexInputField = subIndexInputField;

	if(subIndexNo == 0) {
		let addSubIndexButton = document.createElement('button');
		addSubIndexButton.className = "subindex-add-button";
		addSubIndexButton.title = "Add subindex";
		addSubIndexButton.innerHTML = "+";

		addSubIndexButton.onclick = (event) => {
			let lastSI = container.subitems[container.subitems.length-1].subIndexNo;
			container.appendChild(addSubIndex(container,index,lastSI+1));
			container.object.subitems.push({});
		};

		newSubIndex.appendChild(addSubIndexButton);
	} else {
		let deleteSubIndexButton = document.createElement('button');
		deleteSubIndexButton.className = "subindex-delete-button";
		deleteSubIndexButton.title = "Delete subindex";
		deleteSubIndexButton.innerHTML = "-";

		deleteSubIndexButton.onclick = (event) => {
			container.subitems[newSubIndex.subIndexNo].remove();
			container.subitems.splice(container.subitems[newSubIndex.subIndexNo].subIndexNo,1);
			for(let i = 0; i < container.subitems.length; ++i) {
				container.subitems[i].subIndexNo = i;
				container.subitems[i].subIndexInputField.value = formatHex(i,2);
			}
			container.object.subitems.splice(newSubIndex.subIndexNo,1);
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
			container.remove();
		};

		newSubIndex.appendChild(deleteObjectButton);
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
		if(currentDevice.objects.length > 0) {
			index = container.address.begin + (currentDevice.objects.length*0x10);
		}
		let newSubIndex = addSubIndex(newObject,index,0);
		newObject.appendChild(newSubIndex);
		if(currentDevice.objects === undefined) currentDevice.objects = [];

		currentDevice.objects.push(object);
	}

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

	if(esctool !== undefined) {
		esctool.className = "esctool-control";

		let saveButton = document.createElement("button");
		saveButton.className = "control-button save-button";
		saveButton.innerHTML = "Save";
		saveButton.onclick = (event) => {
			saveSetup();
			writeConsole("Saved set up");
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
			console.log("weeeee");
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
					console.log("Found "+val.name);
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

		let spacerGrp2NewGrp = document.createElement('div');
		spacerGrp2NewGrp.className = 'spacer-1em';

		let spacerGrp2Dev = document.createElement('div');
		spacerGrp2Dev.className = 'spacer-2em';

		let spacerDev2NewDev = document.createElement('div');
		spacerDev2NewDev.className = 'spacer-1em';

		groupsDevices.appendChild(groupsDevicesDiv);

		groupsDevices.appendChild(groupsLabel);
		groupsDevices.appendChild(groupSelector);
		groupsDevices.appendChild(deleteGroupButton);
		groupsDevices.appendChild(spacerGrp2NewGrp);
		groupsDevices.appendChild(groupTypeLabel);
		groupsDevices.appendChild(newGroupTypeInput);
		groupsDevices.appendChild(groupNameLabel);
		groupsDevices.appendChild(newGroupNameInput);
		groupsDevices.appendChild(addGroupButton);

		groupsDevices.appendChild(spacerGrp2Dev);

		groupsDevices.appendChild(devicesLabel);
		groupsDevices.appendChild(deviceSelector);
		groupsDevices.appendChild(deleteDeviceButton);
		groupsDevices.appendChild(spacerDev2NewDev);
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

		let spacerName2PC = document.createElement("div");
		spacerName2PC.className = "spacer-2em";

		let spacerPC2RN = document.createElement("div");
		spacerPC2RN.className = "spacer-2em";

		let configData = document.createElement("div");

		deviceView.appendChild(dvTopLabel);

		deviceView.appendChild(dvDeviceNameLabel);
		deviceView.appendChild(dvDeviceName);
		deviceView.appendChild(spacerName2PC);
		deviceView.appendChild(dvDeviceProductCodeLabel);
		deviceView.appendChild(dvDeviceProductCode);
		deviceView.appendChild(spacerPC2RN);
		deviceView.appendChild(dvDeviceRevisionNoLabel);
		deviceView.appendChild(dvDeviceRevisionNo);
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

function hex2XML(hexStr) {
	let val = parseInt(hexStr);
	if(isNaN(val)) val = 0;
	return "#x"+(val.toString(16).toUpperCase());

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

	let dataTypes = [];



	let XML = new XMLWriter();
	XML.writeStartDocument();
	XML.writeStartElement("EtherCATInfo");
	XML.writeAttributeString("xmlns:xsd","http://www.w3.org/2001/XMLSchema");
	XML.writeAttributeString("xmlns:xsi","http://www.w3.org/2001/XMLSchema-instance");
	XML.writeAttributeString("xsi:noNamespaceSchemaLocation","EtherCATInfo.xsd");
	XML.writeAttributeString("Version","1.6");

	XML.writeStartElement("Vendor");
		XML.writeElementString("ID",hex2XML(setup.setups[selectedSetup].vendorid));
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
			XML.writeStartElement("Device")
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



						XML.writeEndElement(); // DataTypes
					XML.writeEndElement(); // Dictionary
				XML.writeEndElement(); // Profile
			XML.writeEndElement(); // Device
		XML.writeEndElement(); // Devices
	XML.writeEndElement();

	XML.writeEndElement();
	XML.writeEndDocument();
	console.log(XML.flush());
}
