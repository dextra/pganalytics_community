describe('util', function() {
	it('should format', function() {
		expect(i18n.formatter.decimal(10.789, 2)).toBe('10,79');
		expect(i18n.formatter.decimal(10.789, 0)).toBe('11');
		expect(i18n.formatter.decimal(null, 2)).toBe('NULL');
		expect(i18n.formatter.decimal('10x', 2)).toBe('??');
		expect(i18n.formatter.numericUnit(1, 'mb')).toBe('1,0 MB');
		expect(i18n.formatter.numericUnit(5000.57, 'mb')).toBe('5000,6 MB');
		expect(i18n.formatter.numericUnit(5000.55, 'mb')).toBe('5000,6 MB');
		expect(i18n.formatter.numericUnit(5000.54, 'mb')).toBe('5000,5 MB');
		expect(i18n.formatter.numericUnit(9999.9, 'mb')).toBe('9999,9 MB');
		expect(i18n.formatter.numericUnit(9999.0, 'mb')).toBe('9999,0 MB');
		expect(i18n.formatter.numericUnit(10000.9, 'mb')).toBe('10001 MB');
		expect(i18n.formatter.numericUnit(10239.4, 'mb')).toBe('10239 MB');
		expect(i18n.formatter.numericUnit(10239.9, 'mb')).toBe('10240 MB');
		expect(i18n.formatter.numericUnit(10240, 'mb')).toBe('10,0 GB');
		expect(i18n.formatter.numericUnit(1048576, 'mb')).toBe('1,0 TB');
		expect(i18n.formatter.numericUnit(1048576*2, 'mb')).toBe('2,0 TB');
		expect(i18n.formatter.numericUnit(8192, 'bytes')).toBe('8 KB');
		expect(i18n.formatter.numericUnit(8000, 'bytes')).toBe('8000 b');
		expect(i18n.formatter.numericUnit(8.3, 'bytes')).toBe('');
		expect(i18n.formatter.numericUnit(8.3, 'int')).toBe('');
		expect(i18n.formatter.numericUnit(8, 'int')).toBe('8');
		expect(i18n.formatter.numericUnit(123, 's')).toBe('2 min');
		expect(i18n.formatter.numericUnit(53, 's')).toBe('53 s');
		expect(i18n.formatter.numericUnit(3725, 's')).toBe('1h2min');
		expect(i18n.formatter.numericUnit(null)).toBe('NULL');
		expect(i18n.formatter.numericUnit(undefined)).toBe('NULL');
		var dt = new Date('2015-02-28T15:00:00Z');
		expect(i18n.formatter.chartDateTime(dt)).toBe('28/Fev 12:00');
		expect(i18n.formatter.fullDateTime(dt)).toBe('28/02/2015 12:00:00');
		expect(i18n.formatter.chartTime(dt)).toBe('12:00');
		expect(i18n.formatter.chartDateTime(null)).toBe('NULL');
		expect(i18n.formatter.fullDateTime(null)).toBe('NULL');
		expect(i18n.formatter.chartTime(null)).toBe('NULL');
	});
});

describe('user', function() {
	it('should not be logged', function() {
		Cookies.erase("pga");
		expect(Cookies.read("pga")).toBeNull();
		expect(isUserLogged()).toBe(false);
		expect(getUserName()).toBe('NO_USER');
	});
	it('should be logged as admin', function() {
		var login_admin_object = {"u":"admin","t":((new Date()).getTime() - 1000*60)}; /* logged 1 minute ago */
		var login_admin_cookie = btoa(JSON.stringify(login_admin_object)) + ".SIGNATURE_DONT_NEED_TO_BE_VALID";
		Cookies.create("pga", login_admin_cookie, 1);
		Cookies.create("foo", "bar");
		expect(Cookies.read("pga")).toBe(login_admin_cookie);
		expect(isUserLogged()).toBe(true);
		expect(getUserName()).toBe('admin');
	});
	it('should have expired login', function() {
		var login_admin_object = {"u":"admin","t":((new Date()).getTime() - 25*60*60*1000)}; /* logged 25 hours ago */
		var login_admin_cookie = btoa(JSON.stringify(login_admin_object)) + ".SIGNATURE_DONT_NEED_TO_BE_VALID";
		Cookies.create("pga", login_admin_cookie, 1);
		Cookies.create("foo", "bar");
		expect(Cookies.read("pga")).toBe(login_admin_cookie);
		expect(isUserLogged()).toBe(false);
		expect(getUserName()).toBe('admin'); /* getUserName does not care if it is expired, as it is mostly used for display or GA tracking */
	});
});

describe('utility function', function() {
	it('parseURLQuery should return URL params', function() {
		expect(parseURLQuery("?foo=bar&baz=zaz")).toEqual({foo: "bar", baz: "zaz"});
	});
	it('PQQuoteLiteral should quote according to quote_literal', function() {
		expect(PQQuoteLiteral('foo')).toBe('foo');
		expect(PQQuoteLiteral('foo')).not.toBe('"foo"');
		expect(PQQuoteLiteral('Foo')).toBe('"Foo"');
	});
	it('findItemByKeyValue to find', function() {
		var arr = [
			{foo: "fooValue1", bar: "barValue1"},
			{foo: "fooValue2", bar: "barValue2"},
			{foo: "fooValue3", bar: "barValue3"}
		];
		expect(findItemByKeyValue(arr, "foo", "fooValue2")).toBe(1);
		expect(findItemByKeyValue(arr, "foo", "fooValue")).toBe(-1);
		expect(findItemByKeyValue(arr, "foo", "fooValue", 0)).toBe(0);
	});
	it('countObjectKeys to give the count', function() {
		expect(countObjectKeys({foo: 1, bar: 2, zaz: 3})).toBe(3);
		expect(countObjectKeys({})).toBe(0);
		window.testIgnoreObjectKeys = true;
		expect(countObjectKeys({foo: 1, bar: 2, zaz: 3})).toBe(3);
		expect(countObjectKeys({})).toBe(0);
	});
});

