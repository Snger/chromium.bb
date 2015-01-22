if (self.importScripts) {
  importScripts('../resources/fetch-test-helpers.js');
  importScripts('/serviceworker/resources/fetch-access-control-util.js');
}

var TEST_TARGETS = [
  // Auth check
  [BASE_URL + 'Auth',
   [fetchResolved, hasBody], [authCheck1]],
  [BASE_URL + 'Auth&credentials=omit',
   [fetchResolved, hasBody], [checkJsonpError]],
  [BASE_URL + 'Auth&credentials=include',
   [fetchResolved, hasBody], [authCheck1]],
  [BASE_URL + 'Auth&credentials=same-origin',
   [fetchResolved, hasBody], [authCheck1]],

  [BASE_URL + 'Auth&mode=no-cors&credentials=omit',
   [fetchResolved, hasBody], [checkJsonpError]],
  [BASE_URL + 'Auth&mode=no-cors&credentials=include',
   [fetchResolved, hasBody], [authCheck1]],
  [BASE_URL + 'Auth&mode=no-cors&credentials=same-origin',
   [fetchResolved, hasBody], [authCheck1]],

  [BASE_URL + 'Auth&mode=same-origin&credentials=omit',
   [fetchResolved, hasBody], [checkJsonpError]],
  [BASE_URL + 'Auth&mode=same-origin&credentials=include',
   [fetchResolved, hasBody], [authCheck1]],
  [BASE_URL + 'Auth&mode=same-origin&credentials=same-origin',
   [fetchResolved, hasBody], [authCheck1]],

  [BASE_URL + 'Auth&mode=cors&credentials=omit',
   [fetchResolved, hasBody], [checkJsonpError]],
  [BASE_URL + 'Auth&mode=cors&credentials=include',
   [fetchResolved, hasBody], [authCheck1]],
  [BASE_URL + 'Auth&mode=cors&credentials=same-origin',
   [fetchResolved, hasBody], [authCheck1]],

  [OTHER_BASE_URL + 'Auth',
   [fetchResolved, noBody, typeOpaque],
   onlyOnServiceWorkerProxiedTest([authCheck2])],
  [OTHER_BASE_URL + 'Auth&credentials=omit',
   [fetchResolved, noBody, typeOpaque],
   onlyOnServiceWorkerProxiedTest([checkJsonpError])],
  [OTHER_BASE_URL + 'Auth&credentials=include',
   [fetchResolved, noBody, typeOpaque],
   onlyOnServiceWorkerProxiedTest([authCheck2])],
  [OTHER_BASE_URL + 'Auth&credentials=same-origin',
   [fetchResolved, noBody, typeOpaque],
   onlyOnServiceWorkerProxiedTest([authCheck2])],

  [OTHER_BASE_URL + 'Auth&mode=no-cors&credentials=omit',
   [fetchResolved, noBody, typeOpaque],
   onlyOnServiceWorkerProxiedTest([checkJsonpError])],
  [OTHER_BASE_URL + 'Auth&mode=no-cors&credentials=include',
   [fetchResolved, noBody, typeOpaque],
   onlyOnServiceWorkerProxiedTest([authCheck2])],
  [OTHER_BASE_URL + 'Auth&mode=no-cors&credentials=same-origin',
   [fetchResolved, noBody, typeOpaque],
   onlyOnServiceWorkerProxiedTest([authCheck2])],

  [OTHER_BASE_URL + 'Auth&mode=same-origin&credentials=omit',
   [fetchRejected]],
  [OTHER_BASE_URL + 'Auth&mode=same-origin&credentials=include',
   [fetchRejected]],
  [OTHER_BASE_URL + 'Auth&mode=same-origin&credentials=same-origin',
   [fetchRejected]],

  [OTHER_BASE_URL + 'Auth&mode=cors&credentials=omit',
   [fetchRejected]],
  [OTHER_BASE_URL + 'Auth&mode=cors&credentials=include',
   [fetchRejected]],
  [OTHER_BASE_URL + 'Auth&mode=cors&credentials=same-origin',
   [fetchRejected]],

  [OTHER_BASE_URL + 'Auth&mode=cors&credentials=omit&ACAOrigin=*',
   [fetchResolved, hasBody], [checkJsonpError]],

  [OTHER_BASE_URL + 'Auth&mode=cors&credentials=include&ACAOrigin=*',
   [fetchRejected]],
  [OTHER_BASE_URL + 'Auth&mode=cors&credentials=include&ACAOrigin=http://127.0.0.1:8000',
   [fetchRejected]],
  [OTHER_BASE_URL + 'Auth&mode=cors&credentials=include&ACAOrigin=*&ACACredentials=true',
   [fetchRejected]],
  [OTHER_BASE_URL + 'Auth&mode=cors&credentials=include&ACAOrigin=http://127.0.0.1:8000&ACACredentials=true',
   [fetchResolved, hasBody, typeCors], [authCheck2]],

  [OTHER_BASE_URL + 'Auth&mode=cors&credentials=same-origin&ACAOrigin=*',
   [fetchResolved, hasBody], [checkJsonpError]],
];

if (self.importScripts) {
  executeTests(TEST_TARGETS);
  done();
}
