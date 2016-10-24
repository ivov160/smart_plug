$(function(){
	function handle($selector, type) {

		ip = function() {
			return $('.js_ip').val();
		}

		var _this = this;
		$selector.on('click', function($event) {
			var __this = this,
			$elem = $(__this),
			method = (!type ? "GET" : type),
			action = $elem.data('action'),
			url = 'http://' + ip() + '/' + action;

			var $form = $('<form target="display" action="' + url + '" method="' + method + '" />');

			$elem.closest('div').find('.js_param').each(function(index, node) {
				$n = $(node);
				$('<input type="hidden" value="' + $n.val() + '" name="' + $n.attr('name') + '" />').appendTo($form);
			});

			$form.appendTo($('body')).submit().remove();
		});
	};

	function Page()
	{
		var _this = this;

		_this.init = function()	{
			new handle($('.js_info_device'));
			new handle($('.js_info_system'));
			new handle($('.js_power_on'));
			new handle($('.js_power_off'));
			new handle($('.js_power_status'));
			new handle($('.js_wifi_error'));
			new handle($('.js_wifi_scan'));
			new handle($('.js_test_mode_off'));

			new handle($('.js_wifi_settigns'), 'POST');
			new handle($('.js_info_name'), 'POST');
			new handle($('.js_test_mode_on'), 'POST');
		}

		_this.init();
	};
	new Page();
});
