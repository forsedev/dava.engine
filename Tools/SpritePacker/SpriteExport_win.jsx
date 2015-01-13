#target photoshopvar originalUnits = Units.PIXELS;var logFile = null;var processFilePath = arguments[0];var processFileName = arguments[1];function initPlugin(){	originalUnits = preferences.rulerUnits 	preferences.rulerUnits = Units.PIXELS		var dir = new Folder("c:/temp/export");	if (!dir.create())	{		//alert("Cannot create directory c:/temp/export");		}	dir = null;	logFile = File(processFilePath + "$process/" + replaceExtension(documentName, "_log.txt"))	logFile.open("w")	logFile.write ("// Photoshop export plugin\n");	logFile.write ("Input File:" + documentName + "\n");		logFile.write("Dimentions: (" + documentWidth + " x " + documentHeight + ")\n"); }function finishPlugin(){	preferences.rulerUnits = originalUnits	logFile.close()	logFile= null}function writeLog(str){	logFile.write(str);}function replaceExtension(str, ext){	var n = str.indexOf(".", 0);	return str.substr(0, n) + ext;}function nameWithoutExt(str){	var n = str.indexOf(".", 0);	return str.substr(0, n) ;}function saveSelectionToPng(bounds, tempName, fileName){	var saveDoc = app.documents.add(bounds[2].as("px") - bounds[0].as("px"), bounds[3].as("px") - bounds[1].as("px"), 72, "result image",  NewDocumentMode.RGB, DocumentFill.TRANSPARENT);	var newLayerInNewDocument = saveDoc.artLayers.add();	saveDoc.paste();	oldFile = new File(processFilePath + "$process/" + fileName) 	if (oldFile)	{		var v = oldFile.remove()		writeLog("removing file:" + fileName + " val:"  + v + "\n");	}	oldFile = null	outFile = new File(tempName) 	var v = outFile.remove()	writeLog("removing file:" + tempName + " val:"  + v + "\n");	writeLog("Saving to path Check: " + tempName + "\n"); 	writeLog("Saving to path Check: " + fileName + "\n"); 	writeLog("w: " + (bounds[2] - bounds[0]) + " h: " + (bounds[3] - bounds[1]) + "\n"); 		deviceSaveOptions = new ExportOptionsSaveForWeb() 	deviceSaveOptions.format = SaveDocumentType.PNG;	deviceSaveOptions.PNG8 = false;	deviceSaveOptions.quality = 100;	//outFile.open("w")	saveDoc.exportDocument (outFile, ExportType.SAVEFORWEB, deviceSaveOptions);	outFile.rename(fileName);		saveDoc.close(SaveOptions.DONOTSAVECHANGES);	app.activeDocument = mainDocument;}function Log2(value){	result = 1;	while(result < value) result *= 2;	return result;}function getNumberOfFrameLayers(){	cnt = 0;	for (i = artLayers.length - 1; i >= 0; --i)	{		var layer = artLayers[i];		mainDocument.activeLayer = layer				var pathItems = mainDocument.pathItems;		if (pathItems.length == 0)cnt++;	}	return cnt;}function findNumberOfPathLayers(){	cnt = 0;	for (i = artLayers.length - 1; i >= 0; --i)	{		var layer = artLayers[i];		mainDocument.activeLayer = layer				var pathItems = mainDocument.pathItems;		if (pathItems.length != 0)cnt++;	}	return cnt;}function export1PathToFile(file){		for (pathItemIndex = 0; pathItemIndex < mainDocument.pathItems.length; ++pathItemIndex)	{		var pathItem = mainDocument.pathItems[pathItemIndex];		writeLog("Path item kind:" + pathItem.kind + "\n")		writeLog("SubPath item length:" + pathItem.subPathItems.length + "\n")				for (spi = 0; spi < pathItem.subPathItems.length; ++spi)		{			var subPathItem = pathItem.subPathItems[spi];			writeLog("SubPath start:" + subPathItem.pathPoints.length + "\n")							var x = 0;			var y = 0;			file.write(subPathItem.pathPoints.length + "\n");						for (v = 0; v < subPathItem.pathPoints.length; ++v)			{				var pathPoint  = subPathItem.pathPoints[v];				//file.write("XY:" + pathPoint.anchor[0] + "," + pathPoint.anchor[1] + "\n")				x = pathPoint.anchor[0];				y = pathPoint.anchor[1];				file.write(x + " " + y + "\n");			}					}	}		}var dir = new Folder(processFilePath + "$process");if (!dir.create()){	//alert("Cannot create directory processFilePath + $process");	}dir = null;var fileRef = File(processFilePath + processFileName)var mainDocument = app.open(fileRef) app.activeDocument = mainDocument;var documentName = mainDocument.name;var documentWidth = mainDocument.width.as("px");var documentHeight = mainDocument.height.as("px"); function Crop() {	 function cTID(s) 	 { 		 return app.charIDToTypeID(s);      };      function sTID(s) { return app.stringIDToTypeID(s); }; 	 	 var desc001 = new ActionDescriptor(); executeAction( cTID('Crop'), desc001, DialogModes.NO );};//~ var shapeRef = [ //~ 	[0 					, 	0], //~ 	[0 					,	documentHeight],  //~ 	[documentWidth	,	documentHeight], //~ 	[documentWidth	, 	0] //~ ] //~ mainDocument.selection.select(shapeRef);//Crop();//mainDocument.selection.clear();initPlugin();mainDocument.crop(new Array(0,0, documentWidth, documentHeight));writeLog("Arguments: " + arguments[0] + arguments[1] + "\n");defFile = File(processFilePath + "$process/" + replaceExtension(documentName, ".txt"))defFile.open("w")	var artLayers = mainDocument.artLayersframeCount = getNumberOfFrameLayers();defFile.write(documentWidth + " " + documentHeight + "\n");defFile.write(frameCount + "\n")var index = 0;var indexArray = Array(frameCount);for (i = 0; i < frameCount; ++i)indexArray[i] = -1;var pathIndex = 0;for (i = artLayers.length - 1; i >= 0; --i){	app.activeDocument = mainDocument;	// if layer not a background make it visible	var layer = artLayers[i];	mainDocument.activeLayer = layer		// skip bounds layers from export of frames	if (mainDocument.pathItems.length != 0)	{		for (k = index - 1; k >=0; k--)			if (indexArray[k] == -1)				indexArray[k] = pathIndex;				pathIndex ++;		continue;	}	if (!layer.isBackgroundLayer)layer.visible = true	var bounds = layer.bounds;	try	{	layer.copy();	var saveFolder = new Folder(processFilePath + "$process/" );	if (!saveFolder.create())	{			//alert("Cannot create directory " + processFilePath + "$process/");		}	savePath = nameWithoutExt(processFileName) + index + ".png";	tempPath = processFilePath + "$process/temp.png";	//writeLog("Saving to path: " + savePath); 	saveSelectionToPng(bounds, tempPath, savePath)	writeLog("Export Layer Name:" + layer.name + "\n");	offsetX = bounds[0].as("px")	offsetY = bounds[1].as("px")	sizeX = bounds[2].as("px") - bounds[0].as("px")	sizeY = bounds[3].as("px") - bounds[1].as("px")		}catch(err)	{		offsetX = 0		offsetY = 0		sizeX = 1		sizeY = 1	}		defFile.write(offsetX + " " + offsetY + " " +  sizeX  + " " + sizeY +"\n");	index++;}	pathsCount = findNumberOfPathLayers();defFile.write(pathsCount + "\n");if (pathsCount >= 1){	writeLog("Exporting Paths: " + pathsCount + "\n");	index = 0;	for (i = artLayers.length - 1; i >= 0; --i)	{		app.activeDocument = mainDocument;		// if layer not a background make it visible		var layer = artLayers[i];		mainDocument.activeLayer = layer				// skip bounds layers from export of frames		if (mainDocument.pathItems.length == 0)continue;		writeLog("Export path:" + index + "\n")		export1PathToFile(defFile);		index ++;	}		for (i = 0; i < frameCount; ++i)	{		defFile.write(indexArray[i] + "\n");	}}defFile.close()defFile = null;mainDocument.close(SaveOptions.DONOTSAVECHANGES);finishPlugin();/*// Plugin codeinitPlugin();writeLog("Texture pack size: " +xSize + " " + ySize + "\n");//var newLayerInNewDocument = saveDoc.artLayers.add();outFile = new File("c:/temp/export/" + replaceExtension(documentName, ".png")) outFile.remove()pasteY = 0defFile = File("c:/temp/export/" + replaceExtension(documentName, ".txt"))defFile.open("w")	defFile.write(documentWidth + " " + documentHeight + "\n");defFile.write(artLayers.length + "\n")for (i = artLayers.length - 1; i >= 0; --i, index++){	app.activeDocument = mainDocument;	var layer = artLayers[i];	var bounds = layer.bounds;	layer.copy();		var saveDoc = app.documents.add(xSize, ySize,  72, "result image", 	NewDocumentMode.RGB, DocumentFill.TRANSPARENT);	app.activeDocument = saveDoc	var rect = rootNode.RectForFrameIndex(i);		var shapeRef = [ 				[rect.x, 					rect.y], 				[rect.x, 					rect.y + rect.dy],  				[rect.x + rect.dx, 	rect.y + rect.dy], 				[rect.x + rect.dx, 	rect.y] 			] 	x1 = rect.x	y1 = rect.y	x2 = rect.x + rect.dx	y2 = rect.y + rect.dy	offsetX = bounds[0].value	offsetY = bounds[1].value 		defFile.write(x1 + " " + y1 + " " +  x2  + " " + y2 + " " + offsetX + " " + offsetY + "\n");		saveDoc.selection.select(shapeRef) 	saveDoc.paste(true)	pasteY += bounds[3].value - bounds[1].value}	defFile.close()defFile = null;deviceSaveOptions = new ExportOptionsSaveForWeb() deviceSaveOptions.format = SaveDocumentType.PNG;deviceSaveOptions.PNG8 = false;deviceSaveOptions.quality = 100;saveDoc.exportDocument (outFile, ExportType.SAVEFORWEB, deviceSaveOptions);saveDoc.close(SaveOptions.DONOTSAVECHANGES);app.activeDocument = mainDocument;mainDocument.close(SaveOptions.DONOTSAVECHANGES);finishPlugin();*/